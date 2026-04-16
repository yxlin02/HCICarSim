import numpy as np
import pandas as pd
from sklearn.model_selection import StratifiedKFold, cross_validate, cross_val_predict
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LogisticRegression
from sklearn.svm import SVC
from sklearn.neural_network import MLPClassifier
from sklearn.pipeline import Pipeline
from sklearn.metrics import accuracy_score, roc_auc_score, classification_report


def train_and_evaluate(df_reaction_all, features, target, model_type="logistic", n_splits=5, **kwargs):
    X = df_reaction_all[features].copy()
    y = df_reaction_all[target].astype(int)

    if "time_pressure" in X.columns:
        X["time_pressure"] = X["time_pressure"].astype(int)

    if model_type == "logistic":
        clf = LogisticRegression(
            max_iter=1000,
            penalty="l1",
            solver="liblinear",
            class_weight=kwargs.get("class_weight", None),
            random_state=42
        )

    elif model_type == "svm":
        clf = SVC(
            probability=True,
            kernel="rbf",
            class_weight=kwargs.get("class_weight", None),
            random_state=42
        )

    elif model_type == "mlp":
        hidden_layer_sizes = kwargs.get("hidden_layer_sizes", (32, 8))
        clf = MLPClassifier(
            hidden_layer_sizes=hidden_layer_sizes,
            alpha=kwargs.get("alpha", 1e-4),
            early_stopping=True,
            max_iter=1000,
            random_state=42
        )
    else:
        raise ValueError("model_type must be one of: 'logistic', 'svm', 'mlp'")

    pipe = Pipeline([
        ("scaler", StandardScaler()),
        ("clf", clf)
    ])

    cv = StratifiedKFold(n_splits=n_splits, shuffle=True, random_state=42)

    scoring = {
        "accuracy": "accuracy",
        "roc_auc": "roc_auc"
    }

    cv_results = cross_validate(
        pipe,
        X,
        y,
        cv=cv,
        scoring=scoring,
        return_train_score=False
    )

    print("Accuracy per fold:", cv_results["test_accuracy"])
    print("AUC per fold:", cv_results["test_roc_auc"])

    print("Mean Accuracy: %.4f ± %.4f" % (
        np.mean(cv_results["test_accuracy"]),
        np.std(cv_results["test_accuracy"])
    ))
    print("Mean AUC: %.4f ± %.4f" % (
        np.mean(cv_results["test_roc_auc"]),
        np.std(cv_results["test_roc_auc"])
    ))

    # out-of-fold prediction for overall report
    y_pred = cross_val_predict(pipe, X, y, cv=cv, method="predict")
    y_prob = cross_val_predict(pipe, X, y, cv=cv, method="predict_proba")[:, 1]

    print("\nOverall CV Accuracy:", accuracy_score(y, y_pred))
    print("Overall CV AUC:", roc_auc_score(y, y_prob))
    print("\nClassification Report:")
    print(classification_report(y, y_pred))

    if model_type == "logistic":
        df_coef, coefs = get_cv_feature_importance(pipe, X, y, cv)

        print("\nFeature importance (mean ± std):")
        print(df_coef)

    return {
        "cv_results": cv_results,
        "y_true": y,
        "y_pred": y_pred,
        "y_prob": y_prob,
        "feature_importance": df_coef if model_type == "logistic" else None
    }

from sklearn.base import clone

def get_cv_feature_importance(pipe, X, y, cv):
    coefs = []

    for train_idx, test_idx in cv.split(X, y):
        X_train, y_train = X.iloc[train_idx], y.iloc[train_idx]

        model = clone(pipe)
        model.fit(X_train, y_train)

        # 取出 logistic
        clf = model.named_steps["clf"]

        if hasattr(clf, "coef_"):
            coefs.append(clf.coef_[0])

    coefs = np.array(coefs)  # shape: [n_splits, n_features]

    selection_freq = (coefs != 0).mean(axis=0)

    coef_mean = coefs.mean(axis=0)
    coef_std = coefs.std(axis=0)

    df_coef = pd.DataFrame({
        "feature": X.columns,
        "coef_mean": coef_mean,
        "coef_std": coef_std,
        "abs_mean": np.abs(coef_mean),
        "selection_freq": selection_freq
    }).sort_values("abs_mean", ascending=False)

    return df_coef, coefs