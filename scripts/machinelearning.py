import numpy as np
import pandas as pd
from sklearn.model_selection import StratifiedKFold, cross_validate, cross_val_predict
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LogisticRegression
from sklearn.svm import SVC
from sklearn.neural_network import MLPClassifier
from sklearn.pipeline import Pipeline
from sklearn.metrics import accuracy_score, roc_auc_score, classification_report
from sklearn.tree import DecisionTreeClassifier


def train_and_evaluate(
    df_reaction_all,
    features,
    target,
    model_type="logistic",
    n_splits=5,
    return_fold_models=False,
    fit_final_model=True,
    **kwargs
):
    X = df_reaction_all[features].copy()
    y = df_reaction_all[target].astype(int)

    if "time_pressure" in X.columns:
        X["time_pressure"] = X["time_pressure"].astype(int)

    # -----------------------------
    # build classifier
    # -----------------------------
    if model_type == "logistic":
        clf = LogisticRegression(
            max_iter=kwargs.get("max_iter", 1000),
            penalty=kwargs.get("penalty", "l1"),
            solver=kwargs.get("solver", "liblinear"),
            class_weight=kwargs.get("class_weight", None),
            C=kwargs.get("C", 1.0),
            random_state=kwargs.get("random_state", 42),
        )

    elif model_type == "svm":
        clf = SVC(
            probability=True,
            kernel=kwargs.get("kernel", "rbf"),
            C=kwargs.get("C", 1.0),
            gamma=kwargs.get("gamma", "scale"),
            class_weight=kwargs.get("class_weight", None),
            random_state=kwargs.get("random_state", 42),
        )

    elif model_type == "mlp":
        clf = MLPClassifier(
            hidden_layer_sizes=kwargs.get("hidden_layer_sizes", (32, 8)),
            alpha=kwargs.get("alpha", 1e-4),
            early_stopping=kwargs.get("early_stopping", True),
            max_iter=kwargs.get("max_iter", 1000),
            random_state=kwargs.get("random_state", 42),
        )

    elif model_type == "tree":
        clf = DecisionTreeClassifier(
            criterion=kwargs.get("criterion", "gini"),
            splitter=kwargs.get("splitter", "best"),
            max_depth=kwargs.get("max_depth", None),
            min_samples_split=kwargs.get("min_samples_split", 2),
            min_samples_leaf=kwargs.get("min_samples_leaf", 1),
            max_features=kwargs.get("max_features", None),
            class_weight=kwargs.get("class_weight", None),
            ccp_alpha=kwargs.get("ccp_alpha", 0.0),
            random_state=kwargs.get("random_state", 42),
        )
    else:
        raise ValueError("model_type must be one of: 'logistic', 'svm', 'mlp', 'tree'")

    # -----------------------------
    # pipeline
    # tree 其实不需要 scaler，但保留统一接口也可以
    # 如果你想更干净，也可以 tree 不加 scaler
    # -----------------------------
    if model_type == "tree":
        pipe = Pipeline([
            ("clf", clf)
        ])
    else:
        pipe = Pipeline([
            ("scaler", StandardScaler()),
            ("clf", clf)
        ])

    cv = StratifiedKFold(n_splits=n_splits, shuffle=True, random_state=42)

    scoring = {
        "accuracy": "accuracy",
        "roc_auc": "roc_auc"
    }

    # -----------------------------
    # cross-validated metrics
    # -----------------------------
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

    # -----------------------------
    # out-of-fold predictions
    # -----------------------------
    y_pred = cross_val_predict(pipe, X, y, cv=cv, method="predict")
    y_prob = cross_val_predict(pipe, X, y, cv=cv, method="predict_proba")[:, 1]

    print("\nOverall CV Accuracy:", accuracy_score(y, y_pred))
    print("Overall CV AUC:", roc_auc_score(y, y_prob))
    print("\nClassification Report:")
    print(classification_report(y, y_pred))

    # -----------------------------
    # logistic feature importance
    # -----------------------------
    df_coef = None
    if model_type == "logistic":
        df_coef, coefs = get_cv_feature_importance(pipe, X, y, cv)
        print("\nFeature importance (mean ± std):")
        print(df_coef)

    # -----------------------------
    # return fitted fold models if needed
    # -----------------------------
    fold_models = None
    if return_fold_models:
        fold_models = []
        for fold_idx, (train_idx, test_idx) in enumerate(cv.split(X, y)):
            model_fold = clone(pipe)
            model_fold.fit(X.iloc[train_idx], y.iloc[train_idx])
            fold_models.append(model_fold)

    # -----------------------------
    # fit final model on full data
    # for visualization / deployment / inspection
    # -----------------------------
    final_model = None
    if fit_final_model:
        final_model = clone(pipe)
        final_model.fit(X, y)

    return {
        "cv_results": cv_results,
        "y_true": y,
        "y_pred": y_pred,
        "y_prob": y_prob,
        "feature_importance": df_coef,
        "final_model": final_model,      # 全数据拟合后的模型
        "fold_models": fold_models,      # 每折模型（可选）
        "features": features,
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