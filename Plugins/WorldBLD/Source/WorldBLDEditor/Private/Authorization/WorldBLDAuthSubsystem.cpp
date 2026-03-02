// Copyright WorldBLD LLC. All Rights Reserved.

#include "Authorization/WorldBLDAuthSubsystem.h"
#include "Editor.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Async/Async.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Misc/ConfigCacheIni.h"
#include "TimerManager.h"

namespace WorldBLDAuthDebug
{
	static FString SanitizeAuthResponseForLog(const FString& ResponseContent)
	{
		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			return ResponseContent;
		}

		const auto RedactStringField = [](const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName)
		{
			if (Obj.IsValid() && Obj->HasTypedField<EJson::String>(FieldName))
			{
				Obj->SetStringField(FieldName, TEXT("<redacted>"));
			}
		};

		// Common auth token fields we never want to print in full.
		RedactStringField(JsonObject, TEXT("sessionToken"));
		RedactStringField(JsonObject, TEXT("token"));

		// Request secrets we never want to print in logs.
		RedactStringField(JsonObject, TEXT("password"));
		RedactStringField(JsonObject, TEXT("totpCode"));

		if (JsonObject->HasTypedField<EJson::Object>(TEXT("data")))
		{
			const TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
			RedactStringField(DataObject, TEXT("sessionToken"));
			RedactStringField(DataObject, TEXT("token"));
			RedactStringField(DataObject, TEXT("password"));
			RedactStringField(DataObject, TEXT("totpCode"));
		}

		FString Sanitized;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Sanitized);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		return Sanitized;
	}

	static bool TryExtractEmailVerifiedFromUserObject(const TSharedPtr<FJsonObject>& UserObject, bool& OutEmailVerified, FString& OutValueType)
	{
		if (!UserObject.IsValid())
		{
			return false;
		}

		static const TCHAR* const FieldName = TEXT("emailVerified");
		if (UserObject->HasTypedField<EJson::Boolean>(FieldName))
		{
			OutEmailVerified = UserObject->GetBoolField(FieldName);
			OutValueType = TEXT("bool");
			return true;
		}

		if (UserObject->HasTypedField<EJson::String>(FieldName))
		{
			const FString Value = UserObject->GetStringField(FieldName).TrimStartAndEnd().ToLower();
			if (Value == TEXT("true") || Value == TEXT("1"))
			{
				OutEmailVerified = true;
				OutValueType = TEXT("string");
				return true;
			}

			if (Value == TEXT("false") || Value == TEXT("0"))
			{
				OutEmailVerified = false;
				OutValueType = TEXT("string");
				return true;
			}
		}

		if (UserObject->HasTypedField<EJson::Number>(FieldName))
		{
			OutEmailVerified = (UserObject->GetNumberField(FieldName) != 0.0);
			OutValueType = TEXT("number");
			return true;
		}

		return false;
	}

	static bool TryExtractEmailVerified(const TSharedPtr<FJsonObject>& RootObject, bool& OutEmailVerified, FString& OutPath, FString& OutValueType)
	{
		if (!RootObject.IsValid())
		{
			return false;
		}

		if (RootObject->HasTypedField<EJson::Object>(TEXT("user")))
		{
			const TSharedPtr<FJsonObject> UserObject = RootObject->GetObjectField(TEXT("user"));
			if (TryExtractEmailVerifiedFromUserObject(UserObject, OutEmailVerified, OutValueType))
			{
				OutPath = TEXT("user.emailVerified");
				return true;
			}
		}

		// Some endpoints/wrappers return { data: { user: { ... } } }.
		if (RootObject->HasTypedField<EJson::Object>(TEXT("data")))
		{
			const TSharedPtr<FJsonObject> DataObject = RootObject->GetObjectField(TEXT("data"));
			if (DataObject.IsValid() && DataObject->HasTypedField<EJson::Object>(TEXT("user")))
			{
				const TSharedPtr<FJsonObject> UserObject = DataObject->GetObjectField(TEXT("user"));
				if (TryExtractEmailVerifiedFromUserObject(UserObject, OutEmailVerified, OutValueType))
				{
					OutPath = TEXT("data.user.emailVerified");
					return true;
				}
			}
		}

		return false;
	}
}

// Define error codes from backend to match ErrorCode.js
namespace WorldBLDAuthErrors
{
	// Backend error codes (see API responses' "error" field)
	const FString IncorrectEmailOrPassword = TEXT("incorrect-email-or-password");
	const FString UserNotFound = TEXT("user-not-found");
	const FString UserMissingPassword = TEXT("missing-password");
	const FString SecondFactorRequired = TEXT("second-factor-required");
	const FString IncorrectTwoFactorCode = TEXT("incorrect-two-factor-code");
	const FString EmailNotVerified = TEXT("email-not-verified");
	const FString EmailUnverified = TEXT("email-unverified");
	const FString ValidationFailed = TEXT("validation-failed");
	const FString InternalServerError = TEXT("internal-server-error");
}

DEFINE_LOG_CATEGORY_STATIC(LogWorldBLDAuth, Log, All);

namespace WorldBLDAuthConfig
{
	// Temporary escape hatch while diagnosing backend verification state mismatches.
	// Stored in GEditorPerProjectIni under [WorldBLD.Auth].
	static const TCHAR* const BypassEmailVerificationKey = TEXT("bBypassEmailVerification");

	static bool IsBypassEmailVerificationEnabled()
	{
		bool bBypass = false;
		if (GConfig)
		{
			GConfig->GetBool(TEXT("WorldBLD.Auth"), BypassEmailVerificationKey, bBypass, GEditorPerProjectIni);
		}
		return bBypass;
	}
}

void UWorldBLDAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadSessionToken();

	// If user is already logged in on startup, start license polling
	if (!CurrentSessionToken.IsEmpty())
	{
		UE_LOG(LogWorldBLDAuth, Log, TEXT("User already logged in on startup, starting license polling"));
		RefreshLicenses();
	}
}

void UWorldBLDAuthSubsystem::Deinitialize()
{
	StopLicensePolling();
	Super::Deinitialize();
}

void UWorldBLDAuthSubsystem::Login(const FString& Email, const FString& Password, const FString& OTP)
{
	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		static const FString ErrorMessage(TEXT("HTTP Module unavailable"));
		if (IsInGameThread())
		{
			OnLoginFail.Broadcast(ErrorMessage);
			OnLoginFailNative.Broadcast(ErrorMessage);
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				static const FString GTErrorMessage(TEXT("HTTP Module unavailable"));
				OnLoginFail.Broadcast(GTErrorMessage);
				OnLoginFailNative.Broadcast(GTErrorMessage);
			});
		}
		return;
	}

	CachedEmail = Email;
	CachedPassword = Password;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(TEXT("https://worldbld.com/api/auth/desktop/login"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("email"), Email);
	JsonObject->SetStringField(TEXT("password"), Password);
	if (!OTP.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("totpCode"), OTP);
	}

	FString Content;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	Request->SetContentAsString(Content);

	UE_LOG(LogWorldBLDAuth, Log, TEXT("=== LOGIN REQUEST ==="));
	UE_LOG(LogWorldBLDAuth, Log, TEXT("URL: %s"), *Request->GetURL());
	UE_LOG(LogWorldBLDAuth, Log, TEXT("Body (sanitized): %s"), *WorldBLDAuthDebug::SanitizeAuthResponseForLog(Content));

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAuthSubsystem::OnLoginResponseReceived);
	Request->ProcessRequest();
}

void UWorldBLDAuthSubsystem::CheckEmail(const FString& Email)
{
	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		OnEmailCheckFailure.Broadcast(TEXT("HTTP Module unavailable"));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(TEXT("https://worldbld.com/api/auth/desktop/login"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("email"), Email);
	JsonObject->SetStringField(TEXT("password"), TEXT(""));

	FString Content;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	Request->SetContentAsString(Content);

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAuthSubsystem::OnCheckEmailResponseReceived);
	Request->ProcessRequest();
}

void UWorldBLDAuthSubsystem::Logout()
{
	UE_LOG(LogWorldBLDAuth, Log, TEXT("=== LOGOUT ==="));

	// Stop license polling
	StopLicensePolling();

	// Clear authorized tools
	AuthorizedTools.Empty();

	// Clear session token
	CurrentSessionToken.Empty();
	bHasCachedUserCredits = false;
	CachedUserCredits = 0;
	CachedUserName.Empty();

	// Clear config
	if (GConfig)
	{
		GConfig->SetString(TEXT("WorldBLD.Auth"), TEXT("SessionToken"), TEXT(""), GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);
	}

	// Clear cached credentials
	CachedEmail.Empty();
	CachedPassword.Empty();

	UE_LOG(LogWorldBLDAuth, Log, TEXT("User logged out, license polling stopped"));
}

void UWorldBLDAuthSubsystem::OnCheckEmailResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		OnEmailCheckFailure.Broadcast(TEXT("Connection failed"));
		return;
	}

	FString ResponseContent = Response->GetContentAsString();
	int32 ResponseCode = Response->GetResponseCode();

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OnEmailCheckFailure.Broadcast(TEXT("Failed to parse response"));
		return;
	}

	const FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("Unknown error");
	const FString ErrorCode = JsonObject->HasField(TEXT("error")) ? JsonObject->GetStringField(TEXT("error")) : TEXT("");

	if (ResponseCode == 200)
	{
		OnEmailCheckSuccess.Broadcast();
		return;
	}

	if (ResponseCode == 401)
	{
		// The backend uses the "error" field for programmatic codes; "message" is for humans.
		if (ErrorCode == WorldBLDAuthErrors::UserNotFound)
		{
			OnEmailCheckFailure.Broadcast(TEXT("Email not found"));
		}
		else if (
			ErrorCode == WorldBLDAuthErrors::IncorrectEmailOrPassword
			|| ErrorCode == WorldBLDAuthErrors::SecondFactorRequired
			|| ErrorCode == WorldBLDAuthErrors::UserMissingPassword)
		{
			OnEmailCheckSuccess.Broadcast();
		}
		else
		{
			OnEmailCheckFailure.Broadcast(Message);
		}
		return;
	}

	OnEmailCheckFailure.Broadcast(Message.IsEmpty() ? TEXT("Unknown error") : Message);
}

bool UWorldBLDAuthSubsystem::IsLoggedIn()
{
	FString Token;
	if (GConfig)
	{
		GConfig->GetString(TEXT("WorldBLD.Auth"), TEXT("SessionToken"), Token, GEditorPerProjectIni);
	}
	return !Token.IsEmpty();
}

void UWorldBLDAuthSubsystem::ValidateSession()
{
	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		OnSessionInvalid.Broadcast();
		return;
	}

	if (CurrentSessionToken.IsEmpty())
	{
		OnSessionInvalid.Broadcast();
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(TEXT("https://worldbld.com/api/auth/desktop/session"));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Authorization"), CurrentSessionToken);

	UE_LOG(LogWorldBLDAuth, Log, TEXT("=== SESSION VALIDATION REQUEST ==="));
	UE_LOG(LogWorldBLDAuth, Log, TEXT("URL: %s"), *Request->GetURL());
	UE_LOG(LogWorldBLDAuth, Log, TEXT("Token: %s..."), *CurrentSessionToken.Left(20));

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAuthSubsystem::OnSessionValidationReceived);
	Request->ProcessRequest();
}

FString UWorldBLDAuthSubsystem::GetSessionToken() const
{
	return CurrentSessionToken;
}

int32 UWorldBLDAuthSubsystem::GetUserCredits() const
{
	return bHasCachedUserCredits ? CachedUserCredits : 0;
}

void UWorldBLDAuthSubsystem::SetCachedUserCredits(int32 InCredits)
{
	CachedUserCredits = FMath::Max(0, InCredits);
	bHasCachedUserCredits = true;
}

FString UWorldBLDAuthSubsystem::GetUserName() const
{
	return CachedUserName.IsEmpty() ? TEXT("User") : CachedUserName;
}

bool UWorldBLDAuthSubsystem::HasLicenseForTool(const FString& ToolId) const
{
	if (ToolId.IsEmpty())
	{
		return false;
	}
	for (const FString& AuthorizedTool : AuthorizedTools)
	{
		// Check if ToolId is contained within the authorized tool string (case-insensitive)
		if (AuthorizedTool.Contains(ToolId, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogWorldBLDAuth, Log, TEXT("HasLicenseForTool: '%s' found in '%s' - AUTHORIZED"), *ToolId, *AuthorizedTool);
			return true;
		}
	}

	UE_LOG(LogWorldBLDAuth, Log, TEXT("HasLicenseForTool: '%s' NOT found in any authorized tools"), *ToolId);
	return false;
}

TArray<FString> UWorldBLDAuthSubsystem::GetAuthorizedTools() const
{
	return AuthorizedTools;
}

void UWorldBLDAuthSubsystem::RefreshLicenses()
{
	if (CurrentSessionToken.IsEmpty())
	{
		UE_LOG(LogWorldBLDAuth, Warning, TEXT("RefreshLicenses: No session token, cannot refresh"));
		return;
	}

	FHttpModule* Http = &FHttpModule::Get();
	if (!Http)
	{
		UE_LOG(LogWorldBLDAuth, Error, TEXT("RefreshLicenses: HTTP Module unavailable"));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(TEXT("https://worldbld.com/api/auth/desktop/licenses"));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Authorization"), CurrentSessionToken);

	UE_LOG(LogWorldBLDAuth, Log, TEXT("=== REFRESH LICENSES REQUEST ==="));
	UE_LOG(LogWorldBLDAuth, Log, TEXT("URL: %s"), *Request->GetURL());

	Request->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAuthSubsystem::OnLicensesResponseReceived);
	Request->ProcessRequest();
}

void UWorldBLDAuthSubsystem::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	UE_LOG(LogWorldBLDAuth, Log, TEXT("=== LOGIN RESPONSE ==="));
	UE_LOG(LogWorldBLDAuth, Log, TEXT("Connected: %s"), bConnectedSuccessfully ? TEXT("YES") : TEXT("NO"));

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogWorldBLDAuth, Error, TEXT("Connection failed or invalid response"));
		static const FString ErrorMessage(TEXT("Connection failed"));
		if (IsInGameThread())
		{
			OnLoginFail.Broadcast(ErrorMessage);
			OnLoginFailNative.Broadcast(ErrorMessage);
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [this]()
			{
				static const FString GTErrorMessage(TEXT("Connection failed"));
				OnLoginFail.Broadcast(GTErrorMessage);
				OnLoginFailNative.Broadcast(GTErrorMessage);
			});
		}
		return;
	}

	FString ResponseContent = Response->GetContentAsString();
	int32 ResponseCode = Response->GetResponseCode();

	UE_LOG(LogWorldBLDAuth, Log, TEXT("Response Code: %d"), ResponseCode);
	UE_LOG(LogWorldBLDAuth, Log, TEXT("Response Body (sanitized): %s"), *WorldBLDAuthDebug::SanitizeAuthResponseForLog(ResponseContent));

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		const FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("");
		const FString ErrorCode = JsonObject->HasField(TEXT("error")) ? JsonObject->GetStringField(TEXT("error")) : TEXT("");
		if (!ErrorCode.IsEmpty() || !Message.IsEmpty())
		{
			UE_LOG(LogWorldBLDAuth, Log, TEXT("Parsed error/message: error='%s' message='%s'"), *ErrorCode, *Message);
		}

		if (ResponseCode == 200)
		{
			// Success
			if (JsonObject->HasField(TEXT("sessionToken")))
			{
				// Login only returns a token; we must fetch the session to confirm emailVerified before accepting login.
				const FString Token = JsonObject->GetStringField(TEXT("sessionToken"));
				UE_LOG(LogWorldBLDAuth, Log, TEXT("Login returned sessionToken (len=%d, prefix=%s...) - validating session for emailVerified"),
					Token.Len(), *Token.Left(20));

				PendingLoginSessionToken = Token;
				CurrentSessionToken = Token; // In-memory only; do not persist until email verification is confirmed.

				FHttpModule* Http = &FHttpModule::Get();
				if (!Http)
				{
					static const FString ErrorMessage(TEXT("HTTP Module unavailable"));
					const auto BroadcastFail = [this]()
					{
						OnLoginFail.Broadcast(ErrorMessage);
						OnLoginFailNative.Broadcast(ErrorMessage);
					};

					if (IsInGameThread())
					{
						BroadcastFail();
					}
					else
					{
						AsyncTask(ENamedThreads::GameThread, BroadcastFail);
					}
					return;
				}

				TSharedRef<IHttpRequest, ESPMode::ThreadSafe> SessionRequest = Http->CreateRequest();
				SessionRequest->SetURL(TEXT("https://worldbld.com/api/auth/desktop/session"));
				SessionRequest->SetVerb(TEXT("GET"));
				SessionRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
				SessionRequest->SetHeader(TEXT("X-Authorization"), CurrentSessionToken);
				SessionRequest->OnProcessRequestComplete().BindUObject(this, &UWorldBLDAuthSubsystem::OnLoginSessionValidationReceived);
				SessionRequest->ProcessRequest();
			}
			else
			{
				static const FString ErrorMessage(TEXT("Invalid response format"));
				const auto BroadcastFail = [this]()
				{
					OnLoginFail.Broadcast(ErrorMessage);
					OnLoginFailNative.Broadcast(ErrorMessage);
				};

				if (IsInGameThread())
				{
					BroadcastFail();
				}
				else
				{
					AsyncTask(ENamedThreads::GameThread, BroadcastFail);
				}
			}
		}
		else
		{
			// Error or Challenge
			const FString NormalizedMessage = (ErrorCode == WorldBLDAuthErrors::EmailNotVerified || ErrorCode == WorldBLDAuthErrors::EmailUnverified)
				? TEXT("Please check your email for a verification link.")
				: (Message.IsEmpty() ? TEXT("Unknown error") : Message);

			if (ErrorCode == WorldBLDAuthErrors::SecondFactorRequired)
			{
				const auto Broadcast2FA = [this]()
				{
					OnTwoFactorRequired.Broadcast();
					OnTwoFactorRequiredNative.Broadcast();
				};

				if (IsInGameThread())
				{
					Broadcast2FA();
				}
				else
				{
					AsyncTask(ENamedThreads::GameThread, Broadcast2FA);
				}
			}
			else
			{
				const auto BroadcastFail = [this, NormalizedMessage]()
				{
					OnLoginFail.Broadcast(NormalizedMessage);
					OnLoginFailNative.Broadcast(NormalizedMessage);
				};

				if (IsInGameThread())
				{
					BroadcastFail();
				}
				else
				{
					AsyncTask(ENamedThreads::GameThread, BroadcastFail);
				}
			}
		}
	}
	else
	{
		static const FString ErrorMessage(TEXT("Failed to parse response"));
		const auto BroadcastFail = [this]()
		{
			OnLoginFail.Broadcast(ErrorMessage);
			OnLoginFailNative.Broadcast(ErrorMessage);
		};

		if (IsInGameThread())
		{
			BroadcastFail();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, BroadcastFail);
		}
	}
}

void UWorldBLDAuthSubsystem::OnLoginSessionValidationReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	(void)Request;

	UE_LOG(LogWorldBLDAuth, Log, TEXT("=== LOGIN SESSION VALIDATION RESPONSE ==="));
	UE_LOG(LogWorldBLDAuth, Log, TEXT("Connected: %s"), bConnectedSuccessfully ? TEXT("YES") : TEXT("NO"));

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		static const FString ErrorMessage(TEXT("Failed to validate session"));
		PendingLoginSessionToken.Empty();
		CurrentSessionToken.Empty();

		const auto BroadcastFail = [this]()
		{
			OnLoginFail.Broadcast(ErrorMessage);
			OnLoginFailNative.Broadcast(ErrorMessage);
		};

		if (IsInGameThread())
		{
			BroadcastFail();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, BroadcastFail);
		}
		return;
	}

	const int32 ResponseCode = Response->GetResponseCode();
	const FString ResponseContent = Response->GetContentAsString();

	UE_LOG(LogWorldBLDAuth, Log, TEXT("Response Code: %d"), ResponseCode);
	UE_LOG(LogWorldBLDAuth, Log, TEXT("Response Body (sanitized): %s"), *WorldBLDAuthDebug::SanitizeAuthResponseForLog(ResponseContent));

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		static const FString ErrorMessage(TEXT("Failed to validate session"));
		PendingLoginSessionToken.Empty();
		CurrentSessionToken.Empty();

		const auto BroadcastFail = [this]()
		{
			OnLoginFail.Broadcast(ErrorMessage);
			OnLoginFailNative.Broadcast(ErrorMessage);
		};

		if (IsInGameThread())
		{
			BroadcastFail();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, BroadcastFail);
		}
		return;
	}

	if (ResponseCode != 200)
	{
		const FString Message = JsonObject->HasField(TEXT("message")) ? JsonObject->GetStringField(TEXT("message")) : TEXT("Failed to validate session");
		PendingLoginSessionToken.Empty();
		CurrentSessionToken.Empty();

		const auto BroadcastFail = [this, Message]()
		{
			OnLoginFail.Broadcast(Message);
			OnLoginFailNative.Broadcast(Message);
		};

		if (IsInGameThread())
		{
			BroadcastFail();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, BroadcastFail);
		}
		return;
	}

	// Confirm emailVerified from session response: { user: { emailVerified: true/false, ... } }
	bool bEmailVerified = true; // Back-compat: if field missing, do not block login.
	bool bFoundEmailVerified = false;
	FString EmailVerifiedPath;
	FString EmailVerifiedValueType;
	if (JsonObject->HasTypedField<EJson::Object>(TEXT("user")))
	{
		const TSharedPtr<FJsonObject> UserObject = JsonObject->GetObjectField(TEXT("user"));
		bFoundEmailVerified = WorldBLDAuthDebug::TryExtractEmailVerifiedFromUserObject(UserObject, bEmailVerified, EmailVerifiedValueType);
		EmailVerifiedPath = TEXT("user.emailVerified");

		// Cache basic user fields while we're here (mirrors ValidateSession parsing).
		if (UserObject.IsValid())
		{
			if (UserObject->HasField(TEXT("name")))
			{
				CachedUserName = UserObject->GetStringField(TEXT("name"));
			}

			if (UserObject->HasTypedField<EJson::Number>(TEXT("credits")))
			{
				CachedUserCredits = UserObject->GetIntegerField(TEXT("credits"));
				bHasCachedUserCredits = true;
			}
		}
	}
	else
	{
		bFoundEmailVerified = WorldBLDAuthDebug::TryExtractEmailVerified(JsonObject, bEmailVerified, EmailVerifiedPath, EmailVerifiedValueType);
	}

	UE_LOG(LogWorldBLDAuth, Log, TEXT("Email verification extracted: found=%s value=%s path=%s type=%s bypass=%s"),
		bFoundEmailVerified ? TEXT("YES") : TEXT("NO"),
		bEmailVerified ? TEXT("true") : TEXT("false"),
		EmailVerifiedPath.IsEmpty() ? TEXT("<none>") : *EmailVerifiedPath,
		EmailVerifiedValueType.IsEmpty() ? TEXT("<unknown>") : *EmailVerifiedValueType,
		WorldBLDAuthConfig::IsBypassEmailVerificationEnabled() ? TEXT("true") : TEXT("false"));

	if (!bEmailVerified && !WorldBLDAuthConfig::IsBypassEmailVerificationEnabled())
	{
		static const FString ErrorMessage(TEXT("Please verify your email address before logging in."));
		PendingLoginSessionToken.Empty();
		CurrentSessionToken.Empty();

		const auto BroadcastFail = [this]()
		{
			OnLoginFail.Broadcast(ErrorMessage);
			OnLoginFailNative.Broadcast(ErrorMessage);
		};

		if (IsInGameThread())
		{
			BroadcastFail();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, BroadcastFail);
		}
		return;
	}

	// Accept login: persist token, start license polling, broadcast success.
	const FString TokenToPersist = PendingLoginSessionToken;
	PendingLoginSessionToken.Empty();
	SetSessionToken(TokenToPersist);

	StartLicensePolling();
	OnSessionValid.Broadcast();

	const auto BroadcastSuccess = [this, TokenToPersist]()
	{
		OnLoginSuccess.Broadcast(TokenToPersist);
		OnLoginSuccessNative.Broadcast(TokenToPersist);
	};

	if (IsInGameThread())
	{
		BroadcastSuccess();
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, BroadcastSuccess);
	}

	// Clear cached credentials
	CachedEmail.Empty();
	CachedPassword.Empty();
}

void UWorldBLDAuthSubsystem::OnSessionValidationReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	UE_LOG(LogWorldBLDAuth, Log, TEXT("=== SESSION VALIDATION RESPONSE ==="));
	UE_LOG(LogWorldBLDAuth, Log, TEXT("Connected: %s"), bConnectedSuccessfully ? TEXT("YES") : TEXT("NO"));

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogWorldBLDAuth, Error, TEXT("Session validation failed - no connection"));
		OnSessionInvalid.Broadcast();
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	UE_LOG(LogWorldBLDAuth, Log, TEXT("Response Code: %d"), ResponseCode);
	UE_LOG(LogWorldBLDAuth, Log, TEXT("Response Body (sanitized): %s"), *WorldBLDAuthDebug::SanitizeAuthResponseForLog(ResponseContent));

	if (ResponseCode == 200)
	{
		// Parse user data from session response
		// Backend returns: { user: { name: "...", credits: 123, ... } }
		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			bool bEmailVerified = true;
			bool bFoundEmailVerified = false;
			FString EmailVerifiedPath;
			FString EmailVerifiedValueType;
			bFoundEmailVerified = WorldBLDAuthDebug::TryExtractEmailVerified(JsonObject, bEmailVerified, EmailVerifiedPath, EmailVerifiedValueType);
			UE_LOG(LogWorldBLDAuth, Log, TEXT("Session email verification extracted: found=%s value=%s path=%s type=%s bypass=%s"),
				bFoundEmailVerified ? TEXT("YES") : TEXT("NO"),
				bEmailVerified ? TEXT("true") : TEXT("false"),
				EmailVerifiedPath.IsEmpty() ? TEXT("<none>") : *EmailVerifiedPath,
				EmailVerifiedValueType.IsEmpty() ? TEXT("<unknown>") : *EmailVerifiedValueType,
				WorldBLDAuthConfig::IsBypassEmailVerificationEnabled() ? TEXT("true") : TEXT("false"));

			// Check for 'user' object
			if (JsonObject->HasTypedField<EJson::Object>(TEXT("user")))
			{
				TSharedPtr<FJsonObject> UserObject = JsonObject->GetObjectField(TEXT("user"));
				if (UserObject.IsValid())
				{
					// If the backend reports the user's email is not verified, treat this session as invalid and clear stored credentials.
					if (!WorldBLDAuthConfig::IsBypassEmailVerificationEnabled()
						&& UserObject->HasTypedField<EJson::Boolean>(TEXT("emailVerified"))
						&& !UserObject->GetBoolField(TEXT("emailVerified")))
					{
						Logout();
						OnSessionInvalid.Broadcast();
						return;
					}

					// Parse username
					if (UserObject->HasField(TEXT("name")))
					{
						CachedUserName = UserObject->GetStringField(TEXT("name"));
						UE_LOG(LogWorldBLDAuth, Log, TEXT("Cached username: %s"), *CachedUserName);
					}

					// Parse credits
					if (UserObject->HasTypedField<EJson::Number>(TEXT("credits")))
					{
						CachedUserCredits = UserObject->GetIntegerField(TEXT("credits"));
						bHasCachedUserCredits = true;
						UE_LOG(LogWorldBLDAuth, Log, TEXT("Cached credits: %d"), CachedUserCredits);
					}
				}
			}
			// Fallback: Check for legacy formats (data wrapper or direct fields)
			else
			{
				TSharedPtr<FJsonObject> RootOrData = JsonObject;
				if (JsonObject->HasTypedField<EJson::Object>(TEXT("data")))
				{
					RootOrData = JsonObject->GetObjectField(TEXT("data"));
				}

				if (RootOrData.IsValid())
				{
					if (RootOrData->HasTypedField<EJson::Number>(TEXT("credits")))
					{
						CachedUserCredits = RootOrData->GetIntegerField(TEXT("credits"));
						bHasCachedUserCredits = true;
					}
					else if (RootOrData->HasTypedField<EJson::Number>(TEXT("userCredits")))
					{
						CachedUserCredits = RootOrData->GetIntegerField(TEXT("userCredits"));
						bHasCachedUserCredits = true;
					}
				}
			}
		}

		UE_LOG(LogWorldBLDAuth, Log, TEXT("Session is VALID"));
		OnSessionValid.Broadcast();
	}
	else
	{
		bHasCachedUserCredits = false;
		CachedUserCredits = 0;
		UE_LOG(LogWorldBLDAuth, Warning, TEXT("Session is INVALID"));
		OnSessionInvalid.Broadcast();
	}
}

void UWorldBLDAuthSubsystem::OnLicensesResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	UE_LOG(LogWorldBLDAuth, Log, TEXT("=== LICENSES RESPONSE ==="));
	UE_LOG(LogWorldBLDAuth, Log, TEXT("Connected: %s"), bConnectedSuccessfully ? TEXT("YES") : TEXT("NO"));

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogWorldBLDAuth, Error, TEXT("Licenses fetch failed - no connection"));
		return;
	}

	int32 ResponseCode = Response->GetResponseCode();
	FString ResponseContent = Response->GetContentAsString();

	UE_LOG(LogWorldBLDAuth, Log, TEXT("Response Code: %d"), ResponseCode);

	if (ResponseCode != 200)
	{
		UE_LOG(LogWorldBLDAuth, Warning, TEXT("Licenses fetch failed with code %d"), ResponseCode);
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogWorldBLDAuth, Error, TEXT("Failed to parse licenses response"));
		return;
	}

	// Parse the authorizedTools array
	if (JsonObject->HasField(TEXT("authorizedTools")))
	{
		AuthorizedTools.Empty();

		const TArray<TSharedPtr<FJsonValue>>& ToolsArray = JsonObject->GetArrayField(TEXT("authorizedTools"));
		for (const TSharedPtr<FJsonValue>& ToolValue : ToolsArray)
		{
			if (ToolValue->Type == EJson::String)
			{
				AuthorizedTools.Add(ToolValue->AsString());
			}
		}

		UE_LOG(LogWorldBLDAuth, Log, TEXT("=== AUTHORIZED TOOLS UPDATED ==="));
		for (const FString& Tool : AuthorizedTools)
		{
			UE_LOG(LogWorldBLDAuth, Log, TEXT("  - %s"), *Tool);
		}

		// Broadcast the update
		OnLicensesUpdated.Broadcast(AuthorizedTools);
	}
	else
	{
		UE_LOG(LogWorldBLDAuth, Warning, TEXT("Response missing 'authorizedTools' field"));
	}
}

void UWorldBLDAuthSubsystem::SetSessionToken(const FString& Token)
{
	CurrentSessionToken = Token;
	if (GConfig)
	{
		GConfig->SetString(TEXT("WorldBLD.Auth"), TEXT("SessionToken"), *CurrentSessionToken, GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);
	}
}

void UWorldBLDAuthSubsystem::LoadSessionToken()
{
	if (GConfig)
	{
		GConfig->GetString(TEXT("WorldBLD.Auth"), TEXT("SessionToken"), CurrentSessionToken, GEditorPerProjectIni);
	}
}

void UWorldBLDAuthSubsystem::StartLicensePolling()
{
	// Stop any existing timer first
	StopLicensePolling();

	UE_LOG(LogWorldBLDAuth, Log, TEXT("Starting license polling (interval: %.1f seconds)"), LicensePollingInterval);

	// Immediately fetch licenses
	RefreshLicenses();

	// Set up the timer for periodic polling
	if (GEditor)
	{
		GEditor->GetTimerManager()->SetTimer(
			LicensePollingTimerHandle,
			FTimerDelegate::CreateUObject(this, &UWorldBLDAuthSubsystem::OnLicensePollingTick),
			LicensePollingInterval,
			true // Loop
		);
	}
}

void UWorldBLDAuthSubsystem::StopLicensePolling()
{
	if (LicensePollingTimerHandle.IsValid())
	{
		UE_LOG(LogWorldBLDAuth, Log, TEXT("Stopping license polling"));

		if (GEditor)
		{
			GEditor->GetTimerManager()->ClearTimer(LicensePollingTimerHandle);
		}
		LicensePollingTimerHandle.Invalidate();
	}
}

void UWorldBLDAuthSubsystem::OnLicensePollingTick()
{
	UE_LOG(LogWorldBLDAuth, Log, TEXT("License polling tick"));

	// Check if still logged in
	if (CurrentSessionToken.IsEmpty())
	{
		UE_LOG(LogWorldBLDAuth, Warning, TEXT("Session token empty during polling, stopping"));
		StopLicensePolling();
		return;
	}

	RefreshLicenses();
}
