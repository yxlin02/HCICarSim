// Copyright WorldBLD LLC. All Rights Reserved.

#include "ContextMenu/WorldBLDContextMenuFont.h"

#include "Engine/Font.h"

namespace WorldBLDContextMenuFont
{
	static UObject* GetRobotoFontObject()
	{
		static UObject* RobotoFontObject = StaticLoadObject(
			UFont::StaticClass(),
			nullptr,
			TEXT("/Engine/EngineFonts/Roboto.Roboto"));

		return RobotoFontObject;
	}

	static FSlateFontInfo MakeRobotoFontInfo(const FName& InTypefaceFontName, const float InSize, const int32 InLetterSpacing)
	{
		FSlateFontInfo FontInfo;
		FontInfo.FontObject = GetRobotoFontObject();
		FontInfo.FontMaterial = nullptr;

		FFontOutlineSettings OutlineSettings;
		OutlineSettings.OutlineSize = 0;
		OutlineSettings.bMiteredCorners = false;
		OutlineSettings.bSeparateFillAlpha = false;
		OutlineSettings.bApplyOutlineToDropShadows = false;
		OutlineSettings.OutlineMaterial = nullptr;
		OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		FontInfo.OutlineSettings = OutlineSettings;

		FontInfo.TypefaceFontName = InTypefaceFontName;
		FontInfo.Size = InSize;
		FontInfo.LetterSpacing = InLetterSpacing;
		FontInfo.SkewAmount = 0.0f;
		FontInfo.bForceMonospaced = false;
		FontInfo.bMaterialIsStencil = false;
		FontInfo.MonospacedWidth = 1.0f;
		return FontInfo;
	}

	FSlateFontInfo Get()
	{
		return MakeRobotoFontInfo(TEXT("Regular"), 8.0f, 200);
	}

	FSlateFontInfo GetMedium(const float InSize, const int32 InLetterSpacing)
	{
		return MakeRobotoFontInfo(TEXT("Medium"), InSize, InLetterSpacing);
	}

	FSlateFontInfo GetThin(const float InSize, const int32 InLetterSpacing)
	{
		return MakeRobotoFontInfo(TEXT("Thin"), InSize, InLetterSpacing);
	}

	FSlateFontInfo GetTitleBar()
	{
		return MakeRobotoFontInfo(TEXT("Regular"), 10.0f, 250);
	}
}


