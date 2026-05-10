import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from sklearn.model_selection import (
    StratifiedKFold,
    RepeatedStratifiedKFold,
    GroupKFold,
    cross_validate,
    cross_val_predict
)
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LogisticRegression
from sklearn.svm import SVC
from sklearn.neural_network import MLPClassifier
from sklearn.pipeline import Pipeline
from sklearn.metrics import accuracy_score, roc_auc_score, classification_report
from sklearn.tree import DecisionTreeClassifier
from sklearn.ensemble import RandomForestClassifier,  HistGradientBoostingClassifier
from sklearn.metrics import (
    accuracy_score,
    balanced_accuracy_score,
    f1_score,
    precision_score,
    recall_score,
    roc_auc_score,
    brier_score_loss,
    roc_curve, 
    auc,
    classification_report
)

def make_leave_subject_splits(df, group_col="sub_id", leave_subjects=None):
    """
    Custom subject-level CV splitter.

    Parameters
    ----------
    df : pd.DataFrame
        Full dataframe.
    group_col : str
        Subject/group column name.
    leave_subjects : None, list, or list of lists
        - None:
            standard LOSO, leave one subject out each fold.
        - ["sub01", "sub03"]&#58;             only evaluate selected subjects, one subject per fold.
        - [["sub01", "sub02"], ["sub05", "sub08"]]:
            custom grouped holdout folds.

    Returns
    -------
    splits : list of (train_idx, test_idx)
    """

    all_subjects = pd.Series(df[group_col].unique())

    if leave_subjects is None:
        # Standard LOSO: each subject is one test fold
        leave_subjects = [[s] for s in all_subjects]

    else:
        # If user passes a flat list, convert to one-subject-per-fold
        # Example: ["S01", "S02"] -> [["S01"], ["S02"]]
        if len(leave_subjects) > 0 and not isinstance(leave_subjects[0], (list, tuple, set)):
            leave_subjects = [[s] for s in leave_subjects]

    splits = []

    for fold_subjects in leave_subjects:
        fold_subjects = list(fold_subjects)

        test_mask = df[group_col].isin(fold_subjects).values
        train_mask = ~test_mask

        train_idx = np.where(train_mask)[0]
        test_idx = np.where(test_mask)[0]

        if len(test_idx) == 0:
            print(f"Warning: no rows found for held-out subjects: {fold_subjects}")
            continue

        splits.append((train_idx, test_idx))

    return splits


def train_and_evaluate(
    df_reaction_all,
    features,
    target,
    model_type="logistic",
    n_splits=5,
    n_repeats=1,
    repeated_cv=False,
    return_fold_models=False,
    fit_final_model=True,

    # new arguments
    cv_type="stratified",       # "stratified", "groupkfold", "loso", "custom_loso"
    group_col="sub_id",
    leave_subjects=None,

    **kwargs
):
    X = df_reaction_all[features].copy()
    y = df_reaction_all[target].astype(int)
    groups = df_reaction_all[group_col]

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

    elif model_type == "forest":
        clf = RandomForestClassifier(
            n_estimators=kwargs.get("n_estimators", 500),
            criterion=kwargs.get("criterion", "gini"),
            max_depth=kwargs.get("max_depth", None),
            min_samples_split=kwargs.get("min_samples_split", 2),
            min_samples_leaf=kwargs.get("min_samples_leaf", 1),
            max_features=kwargs.get("max_features", "sqrt"),
            bootstrap=kwargs.get("bootstrap", True),
            class_weight=kwargs.get("class_weight", "balanced"),
            oob_score=kwargs.get("oob_score", False),
            n_jobs=kwargs.get("n_jobs", -1),
            random_state=kwargs.get("random_state", 42),
            ccp_alpha=kwargs.get("ccp_alpha", 0.0),
        )

    elif model_type == "gbdt":
        clf = HistGradientBoostingClassifier(
            max_iter=kwargs.get("max_iter", 500),
            learning_rate=kwargs.get("learning_rate", 0.03),
            max_leaf_nodes=kwargs.get("max_leaf_nodes", 31),
            max_depth=kwargs.get("max_depth", 3),
            l2_regularization=kwargs.get("l2_regularization", 1.0),
            class_weight=kwargs.get("class_weight", "balanced"),
            random_state=kwargs.get("random_state", 42),
        )

    else:
        raise ValueError(
            "model_type must be one of: "
            "'logistic', 'svm', 'mlp', 'tree', 'forest', 'gbdt'"
        )

    pipe = Pipeline([
        ("clf", clf)
    ])

    # -----------------------------
    # choose CV scheme
    # -----------------------------
    if cv_type == "stratified":
        if repeated_cv:
            cv = RepeatedStratifiedKFold(
                n_splits=n_splits,
                n_repeats=n_repeats,
                random_state=kwargs.get("random_state", 42)
            )
            cv_splits = cv
            cv_groups = None
        else:
            cv = StratifiedKFold(
                n_splits=n_splits,
                shuffle=True,
                random_state=kwargs.get("random_state", 42)
            )
            cv_splits = cv
            cv_groups = None

    elif cv_type == "groupkfold":
        cv = GroupKFold(n_splits=n_splits)
        cv_splits = cv
        cv_groups = groups

    elif cv_type in ["loso", "custom_loso"]:
        cv_splits = make_leave_subject_splits(
            df_reaction_all,
            group_col=group_col,
            leave_subjects=leave_subjects
        )
        cv_groups = None

    else:
        raise ValueError(
            "cv_type must be one of: "
            "'stratified', 'groupkfold', 'loso', 'custom_loso'"
        )

    scoring = {
        "accuracy": "accuracy",
        "balanced_accuracy": "balanced_accuracy",
        "roc_auc": "roc_auc",
        "f1": "f1",
        "precision": "precision",
        "recall": "recall",
    }

    # -----------------------------
    # cross-validated metrics
    # -----------------------------
    if cv_type in ["groupkfold"]:
        cv_results = cross_validate(
            pipe,
            X,
            y,
            cv=cv_splits,
            groups=cv_groups,
            scoring=scoring,
            return_train_score=False,
            error_score=np.nan
        )
    else:
        cv_results = cross_validate(
            pipe,
            X,
            y,
            cv=cv_splits,
            scoring=scoring,
            return_train_score=False,
            error_score=np.nan
        )

    print("Accuracy per fold:", cv_results["test_accuracy"])
    print("AUC per fold:", cv_results["test_roc_auc"])

    print("Mean Accuracy: %.4f ± %.4f" % (
        np.nanmean(cv_results["test_accuracy"]),
        np.nanstd(cv_results["test_accuracy"])
    ))
    print("Mean AUC: %.4f ± %.4f" % (
        np.nanmean(cv_results["test_roc_auc"]),
        np.nanstd(cv_results["test_roc_auc"])
    ))

    # -----------------------------
    # out-of-fold predictions
    # -----------------------------
    if cv_type == "stratified" and repeated_cv:
        y_pred, y_prob = repeated_cv_predict(pipe, X, y, cv_splits)

    else:
        if cv_type == "groupkfold":
            y_pred = cross_val_predict(
                pipe, X, y,
                cv=cv_splits,
                groups=cv_groups,
                method="predict"
            )
            y_prob = cross_val_predict(
                pipe, X, y,
                cv=cv_splits,
                groups=cv_groups,
                method="predict_proba"
            )[:, 1]

        else:
            y_pred = cross_val_predict(
                pipe, X, y,
                cv=cv_splits,
                method="predict"
            )
            y_prob = cross_val_predict(
                pipe, X, y,
                cv=cv_splits,
                method="predict_proba"
            )[:, 1]

    print("\nOverall CV Accuracy:", accuracy_score(y, y_pred))
    print("Overall CV AUC:", roc_auc_score(y, y_prob))
    print("\nClassification Report:")
    print(classification_report(y, y_pred))

    # -----------------------------
    # logistic feature importance
    # -----------------------------
    df_coef = None
    if model_type == "logistic":
        if cv_type == "groupkfold":
            df_coef, coefs = get_cv_feature_importance(
                pipe, X, y, cv_splits, groups=cv_groups
            )
        else:
            df_coef, coefs = get_cv_feature_importance(
                pipe, X, y, cv_splits
            )

        print("\nFeature importance (mean ± std):")
        print(df_coef)

    # -----------------------------
    # return fitted fold models if needed
    # -----------------------------
    fold_models = None
    fold_info = []

    if return_fold_models:
        fold_models = []

        if cv_type == "groupkfold":
            split_iter = cv_splits.split(X, y, groups=cv_groups)
        else:
            split_iter = cv_splits.split(X, y) if hasattr(cv_splits, "split") else cv_splits

        for fold_idx, (train_idx, test_idx) in enumerate(split_iter):
            model_fold = clone(pipe)
            model_fold.fit(X.iloc[train_idx], y.iloc[train_idx])
            fold_models.append(model_fold)

            fold_subjects = df_reaction_all.iloc[test_idx][group_col].unique().tolist()
            fold_info.append({
                "fold": fold_idx,
                "test_subjects": fold_subjects,
                "n_train": len(train_idx),
                "n_test": len(test_idx),
            })

    else:
        # still store fold info even if models are not returned
        if cv_type == "groupkfold":
            split_iter = cv_splits.split(X, y, groups=cv_groups)
        else:
            split_iter = cv_splits.split(X, y) if hasattr(cv_splits, "split") else cv_splits

        for fold_idx, (train_idx, test_idx) in enumerate(split_iter):
            fold_subjects = df_reaction_all.iloc[test_idx][group_col].unique().tolist()
            fold_info.append({
                "fold": fold_idx,
                "test_subjects": fold_subjects,
                "n_train": len(train_idx),
                "n_test": len(test_idx),
            })

    fold_info = pd.DataFrame(fold_info)

    # -----------------------------
    # fit final model on full data
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
        "final_model": final_model,
        "fold_models": fold_models,
        "fold_info": fold_info,
        "features": features,
        "cv_type": cv_type,
        "group_col": group_col,
        "leave_subjects": leave_subjects,
    }

from sklearn.base import clone
import numpy as np

def repeated_cv_predict(pipe, X, y, cv, threshold=0.5):
    """
    Repeated CV version of cross_val_predict.

    Each sample may appear in the test set multiple times across repeats.
    We average predicted probabilities across all test appearances.
    """
    y_prob_sum = np.zeros(len(y), dtype=float)
    counts = np.zeros(len(y), dtype=int)

    for train_idx, test_idx in cv.split(X, y):
        model = clone(pipe)
        model.fit(X.iloc[train_idx], y.iloc[train_idx])

        if hasattr(model, "predict_proba"):
            prob = model.predict_proba(X.iloc[test_idx])[:, 1]
        else:
            raise ValueError("The model does not support predict_proba.")

        y_prob_sum[test_idx] += prob
        counts[test_idx] += 1

    if np.any(counts == 0):
        raise ValueError("Some samples were never included in a test fold.")

    y_prob_avg = y_prob_sum / counts
    y_pred_avg = (y_prob_avg >= threshold).astype(int)

    return y_pred_avg, y_prob_avg

def get_cv_feature_importance(pipe, X, y, cv, groups=None):
    from sklearn.base import clone
    import numpy as np
    import pandas as pd

    coefs = []

    if hasattr(cv, "split"):
        if groups is not None:
            split_iter = cv.split(X, y, groups=groups)
        else:
            split_iter = cv.split(X, y)
    else:
        split_iter = cv

    for train_idx, test_idx in split_iter:
        model = clone(pipe)
        model.fit(X.iloc[train_idx], y.iloc[train_idx])

        clf = model.named_steps["clf"]

        if hasattr(clf, "coef_"):
            coefs.append(clf.coef_[0])

    coefs = np.asarray(coefs)

    df_coef = pd.DataFrame({
        "feature": X.columns,
        "coef_mean": coefs.mean(axis=0),
        "coef_std": coefs.std(axis=0),
        "abs_mean": np.abs(coefs).mean(axis=0),
        "selection_freq": (coefs != 0).mean(axis=0),
    })

    df_coef = df_coef.sort_values("abs_mean", ascending=False).reset_index(drop=True)

    return df_coef, coefs

def summarize_model_results(model_results):
    rows = []

    for model_name, res in model_results.items():
        y_true = np.asarray(res["y_true"])
        y_pred = np.asarray(res["y_pred"])
        y_prob = np.asarray(res["y_prob"])

        row = {
            "model": model_name,

            # overall out-of-fold performance
            "accuracy_oof": accuracy_score(y_true, y_pred),
            "balanced_accuracy_oof": balanced_accuracy_score(y_true, y_pred),
            "auc_oof": roc_auc_score(y_true, y_prob),
            "f1_oof": f1_score(y_true, y_pred),
            "precision_oof": precision_score(y_true, y_pred),
            "recall_oof": recall_score(y_true, y_pred),
            "brier_oof": brier_score_loss(y_true, y_prob),
        }

        # fold-level CV metrics
        cv_results = res["cv_results"]

        for key in cv_results:
            if key.startswith("test_"):
                metric_name = key.replace("test_", "")
                values = cv_results[key]

                row[f"{metric_name}_mean"] = np.mean(values)
                row[f"{metric_name}_std"] = np.std(values, ddof=1)
                row[f"{metric_name}_se"] = np.std(values, ddof=1) / np.sqrt(len(values))

        rows.append(row)

    df_summary = pd.DataFrame(rows)

    # 默认按 AUC 排序
    if "roc_auc_mean" in df_summary.columns:
        df_summary = df_summary.sort_values("roc_auc_mean", ascending=False)
    else:
        df_summary = df_summary.sort_values("auc_oof", ascending=False)

    return df_summary

def fit_surrogate_tree_from_forest(
    df,
    features,
    res_forest,
    max_depth=3,
    min_samples_leaf=10,
    random_state=42,
):
    X = df[features].copy()

    if "time_pressure" in X.columns:
        X["time_pressure"] = X["time_pressure"].astype(int)

    forest_model = res_forest["final_model"]

    # random forest 的 hard prediction 和 probability
    rf_pred = forest_model.predict(X)
    rf_prob = forest_model.predict_proba(X)[:, 1]

    surrogate = DecisionTreeClassifier(
        max_depth=max_depth,
        min_samples_leaf=min_samples_leaf,
        random_state=random_state,
    )

    surrogate.fit(X, rf_pred)

    surrogate_pred = surrogate.predict(X)
    surrogate_prob = surrogate.predict_proba(X)[:, 1]

    fidelity_acc = accuracy_score(rf_pred, surrogate_pred)

    try:
        fidelity_auc = roc_auc_score(rf_pred, surrogate_prob)
    except Exception:
        fidelity_auc = np.nan

    return {
        "surrogate_tree": surrogate,
        "rf_pred": rf_pred,
        "rf_prob": rf_prob,
        "surrogate_pred": surrogate_pred,
        "surrogate_prob": surrogate_prob,
        "fidelity_accuracy": fidelity_acc,
        "fidelity_auc": fidelity_auc,
    }

# =======================================
# plotter
# =======================================
def plot_logistic_loadings(res_logistic, top_n=None, figsize=(6,4)):
    df_coef = res_logistic["feature_importance"].copy()

    if top_n is not None:
        df_coef = df_coef.head(top_n)

    # 为了图上从小到大排列
    df_coef = df_coef.sort_values("coef_mean", ascending=True)

    plt.figure(figsize=(figsize[0], max(figsize[1], 0.35 * len(df_coef))))

    y = np.arange(len(df_coef))

    plt.barh(
        y,
        df_coef["coef_mean"],
        xerr=df_coef["coef_std"],
        alpha=1,
        capsize=3,
        edgecolor="black",
        facecolor="none",
        linewidth=1.2
    )

    plt.axvline(0, linestyle="--", linewidth=2, zorder=3)
    plt.yticks(y, df_coef["feature"])
    plt.xlabel("Standardized logistic coefficient")
    plt.title("Logistic Regression Loadings")
    # plt.tight_layout()
    plt.show()

def make_performance_long_df(model_results, metrics=None):
    if metrics is None:
        metrics = {
            "roc_auc": "AUC",
            "balanced_accuracy": "Balanced Accuracy",
            "accuracy": "Accuracy",
            "f1": "F1",
        }

    rows = []

    for model_name, res in model_results.items():
        cv_results = res["cv_results"]

        for metric_key, metric_label in metrics.items():
            cv_key = f"test_{metric_key}"

            if cv_key not in cv_results:
                continue

            scores = cv_results[cv_key]

            for fold_idx, score in enumerate(scores):
                rows.append({
                    "model": model_name,
                    "metric": metric_label,
                    "fold": fold_idx,
                    "score": score,
                })

    return pd.DataFrame(rows)


def plot_model_performance(
    df_perf_long,
    metrics_to_plot=("AUC", "Balanced Accuracy"),
    model_order=None,
    plot_style="box",
    b_show_scatter=False,
    figsize = (6,4)
):
    df_plot = df_perf_long[df_perf_long["metric"].isin(metrics_to_plot)].copy()

    if model_order is None:
        model_order = list(df_plot["model"].unique())

    metrics = list(metrics_to_plot)

    fig, axes = plt.subplots(
        1,
        len(metrics),
        figsize=(figsize[0] * len(metrics), figsize[1]),
        sharey=True
    )

    if len(metrics) == 1:
        axes = [axes]

    for ax, metric in zip(axes, metrics):
        d = df_plot[df_plot["metric"] == metric]

        data = [
            d[d["model"] == model]["score"].dropna().values
            for model in model_order
        ]

        if plot_style == "box":
            ax.boxplot(
                data,
                labels=model_order,
                showmeans=True,
                meanline=False,
                patch_artist=False,
            )

        elif plot_style == "violin":
            parts = ax.violinplot(
                data,
                showmeans=True,
                showmedians=False,
                showextrema=True
            )

            for body in parts["bodies"]:
                body.set_alpha(0.6)

        if b_show_scatter:
            # add split-level points
            for i, scores in enumerate(data, start=1):
                jitter = np.random.normal(loc=0, scale=0.04, size=len(scores))
                ax.scatter(
                    np.repeat(i, len(scores)) + jitter,
                    scores,
                    alpha=0.45,
                    s=22,
                )

        ax.axhline(0.5, linestyle="--", linewidth=1)
        ax.set_title(metric)
        ax.set_ylabel("Score")
        ax.set_ylim(0.45, 1.0)
        ax.set_xticklabels(model_order, ha="right", )
        ax.tick_params(axis="x", rotation=35)

    plt.tight_layout()
    plt.show()

def plot_oof_roc_curves(model_results, figsize=(6, 5)):
    plt.figure(figsize=figsize)

    for model_name, res in model_results.items():
        y_true = np.asarray(res["y_true"])
        y_prob = np.asarray(res["y_prob"])

        fpr, tpr, _ = roc_curve(y_true, y_prob)
        auc_score = roc_auc_score(y_true, y_prob)

        plt.plot(fpr, tpr, label=f"{model_name} (AUC = {auc_score:.3f})")

    plt.plot([0, 1], [0, 1], linestyle="--", linewidth=1)
    plt.xlabel("False Positive Rate")
    plt.ylabel("True Positive Rate")
    plt.title("Out-of-fold ROC Curves")
    # plt.legend(frameon=False)
    plt.legend(bbox_to_anchor=(1.0,0), loc="lower left",frameon=False)
    # plt.tight_layout()
    plt.show()

# =======================================
# test
# =======================================
from scipy.stats import wilcoxon, ttest_1samp, ttest_rel
from statsmodels.stats.multitest import multipletests

def test_model_predictability(model_results, metric="roc_auc", chance=0.5, method="wilcoxon"):
    rows = []

    for model_name, res in model_results.items():
        cv_key = f"test_{metric}"

        if cv_key not in res["cv_results"]:
            continue

        scores = np.asarray(res["cv_results"][cv_key])

        if method == "wilcoxon":
            # alternative="greater": test whether scores > chance
            stat, p = wilcoxon(scores - chance, alternative="greater")
            test_name = "Wilcoxon signed-rank"
        elif method == "ttest":
            stat, p = ttest_1samp(scores, chance, alternative="greater")
            test_name = "one-sample t-test"
        else:
            raise ValueError("method must be 'wilcoxon' or 'ttest'.")

        rows.append({
            "model": model_name,
            "metric": metric,
            "chance": chance,
            "mean": scores.mean(),
            "std": scores.std(ddof=1),
            "n_folds": len(scores),
            "test": test_name,
            "stat": stat,
            "p_raw": p,
        })

    df = pd.DataFrame(rows)

    reject, p_fdr, _, _ = multipletests(df["p_raw"], method="fdr_bh")
    df["p_fdr"] = p_fdr
    df["significant_fdr"] = reject

    return df.sort_values("mean", ascending=False)

from itertools import combinations

def compare_models_pairwise(
    model_results,
    metric="roc_auc",
    method="wilcoxon",
    correction="fdr_bh",
):
    rows = []

    for model_a, model_b in combinations(model_results.keys(), 2):
        cv_key = f"test_{metric}"

        scores_a = np.asarray(model_results[model_a]["cv_results"][cv_key])
        scores_b = np.asarray(model_results[model_b]["cv_results"][cv_key])

        if len(scores_a) != len(scores_b):
            raise ValueError(f"{model_a} and {model_b} have different number of folds.")

        diff = scores_a - scores_b

        if method == "wilcoxon":
            stat, p = wilcoxon(scores_a, scores_b, alternative="two-sided")
            test_name = "paired Wilcoxon signed-rank"
        elif method == "ttest":
            stat, p = ttest_rel(scores_a, scores_b)
            test_name = "paired t-test"
        else:
            raise ValueError("method must be 'wilcoxon' or 'ttest'.")

        rows.append({
            "model_a": model_a,
            "model_b": model_b,
            "metric": metric,
            "mean_a": scores_a.mean(),
            "mean_b": scores_b.mean(),
            "mean_diff_a_minus_b": diff.mean(),
            "std_diff": diff.std(ddof=1),
            "n_folds": len(scores_a),
            "test": test_name,
            "stat": stat,
            "p_raw": p,
        })

    df = pd.DataFrame(rows)

    reject, p_adj, _, _ = multipletests(df["p_raw"], method=correction)
    df["p_fdr"] = p_adj
    df["significant_fdr"] = reject

    return df.sort_values("p_fdr")

def compare_models_to_baseline(
    model_results,
    baseline="Logistic",
    metric="roc_auc",
    method="wilcoxon",
    correction="fdr_bh",
):
    rows = []
    cv_key = f"test_{metric}"

    baseline_scores = np.asarray(model_results[baseline]["cv_results"][cv_key])

    for model_name, res in model_results.items():
        if model_name == baseline:
            continue

        scores = np.asarray(res["cv_results"][cv_key])
        diff = scores - baseline_scores

        if method == "wilcoxon":
            stat, p = wilcoxon(scores, baseline_scores, alternative="two-sided")
            test_name = "paired Wilcoxon signed-rank"
        elif method == "ttest":
            stat, p = ttest_rel(scores, baseline_scores)
            test_name = "paired t-test"
        else:
            raise ValueError("method must be 'wilcoxon' or 'ttest'.")

        rows.append({
            "model": model_name,
            "baseline": baseline,
            "metric": metric,
            "model_mean": scores.mean(),
            "baseline_mean": baseline_scores.mean(),
            "mean_diff_vs_baseline": diff.mean(),
            "std_diff": diff.std(ddof=1),
            "n_folds": len(scores),
            "test": test_name,
            "stat": stat,
            "p_raw": p,
        })

    df = pd.DataFrame(rows)
    reject, p_adj, _, _ = multipletests(df["p_raw"], method=correction)
    df["p_fdr"] = p_adj
    df["significant_fdr"] = reject

    return df.sort_values("p_fdr")