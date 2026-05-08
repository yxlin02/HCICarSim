import numpy as np
import pandas as pd

from itertools import combinations
from scipy.stats import ttest_rel, wilcoxon
from statsmodels.stats.multitest import multipletests


def paired_compare_models(
    df,
    metric="acc",
    baseline_model=None,
    test_method="ttest",
    b_fdr=True,
    alpha=0.05,
    model_col="model",
    repeat_col="repeat",
    fold_col="fold",
    display_result=True,
):
    """
    Paired comparison across repeated CV folds.

    Parameters
    ----------
    df : pd.DataFrame
        Result dataframe containing model, repeat, fold, and metric columns.

    metric : str
        Metric to compare, e.g., "acc", "auc", "logloss".

    baseline_model : str or None
        If None, perform all pairwise model comparisons.
        If provided, compare every other model against this baseline model.

    test_method : {"ttest", "wilcoxon", "both"}
        Statistical test method.

    b_fdr : bool
        Whether to apply FDR correction across all comparisons for this metric.

    alpha : float
        Significance threshold for creating significance flags.

    display_result : bool
        Whether to display the result dataframe.

    Returns
    -------
    res : pd.DataFrame
        Comparison results.
    """

    assert test_method in ["ttest", "wilcoxon", "both"], \
        "test_method must be one of: 'ttest', 'wilcoxon', 'both'."

    required_cols = [model_col, repeat_col, fold_col, metric]
    missing_cols = [c for c in required_cols if c not in df.columns]
    if len(missing_cols) > 0:
        raise ValueError(f"Missing required columns: {missing_cols}")

    models = list(pd.unique(df[model_col].dropna()))

    if baseline_model is not None:
        if baseline_model not in models:
            raise ValueError(
                f"baseline_model='{baseline_model}' not found in df[{model_col}]. "
                f"Available models: {models}"
            )

        comparisons = [
            (m, baseline_model)
            for m in models
            if m != baseline_model
        ]
    else:
        comparisons = list(combinations(models, 2))

    rows = []

    for model_1, model_2 in comparisons:
        df1 = (
            df[df[model_col] == model_1]
            [[repeat_col, fold_col, metric]]
            .rename(columns={metric: "score_1"})
        )

        df2 = (
            df[df[model_col] == model_2]
            [[repeat_col, fold_col, metric]]
            .rename(columns={metric: "score_2"})
        )

        merged = pd.merge(
            df1,
            df2,
            on=[repeat_col, fold_col],
            how="inner"
        ).dropna(subset=["score_1", "score_2"])

        x = merged["score_1"].astype(float).values
        y = merged["score_2"].astype(float).values
        diff = x - y

        n_pairs = len(merged)

        base_info = {
            "metric": metric,
            "model_1": model_1,
            "model_2": model_2,
            "comparison": f"{model_1} - {model_2}",
            "n_pairs": n_pairs,
            "model_1_mean": np.nanmean(x) if n_pairs > 0 else np.nan,
            "model_2_mean": np.nanmean(y) if n_pairs > 0 else np.nan,
            "mean_diff_1_minus_2": np.nanmean(diff) if n_pairs > 0 else np.nan,
            "std_diff": np.nanstd(diff, ddof=1) if n_pairs > 1 else np.nan,
        }

        # paired t-test
        if test_method in ["ttest", "both"]:
            if n_pairs < 2 or np.allclose(diff, diff[0]):
                stat, pval = np.nan, np.nan
            else:
                stat, pval = ttest_rel(x, y, nan_policy="omit")

            row = base_info.copy()
            row.update({
                "test_method": "paired_ttest",
                "stat_name": "t",
                "stat": stat,
                "pval": pval,
            })
            rows.append(row)

        # Wilcoxon signed-rank test
        if test_method in ["wilcoxon", "both"]:
            if n_pairs < 2 or np.allclose(diff, 0):
                stat, pval = np.nan, np.nan
            else:
                try:
                    stat, pval = wilcoxon(x, y)
                except ValueError:
                    stat, pval = np.nan, np.nan

            row = base_info.copy()
            row.update({
                "test_method": "wilcoxon",
                "stat_name": "W",
                "stat": stat,
                "pval": pval,
            })
            rows.append(row)

    res = pd.DataFrame(rows)

    if len(res) == 0:
        if display_result:
            display(res)
        return res

    # FDR correction
    if b_fdr:
        res["p_fdr"] = np.nan
        valid = res["pval"].notna()

        if valid.sum() > 0:
            res.loc[valid, "p_fdr"] = multipletests(
                res.loc[valid, "pval"].values,
                alpha=alpha,
                method="fdr_bh"
            )[1]
    else:
        res["p_fdr"] = res["pval"]

    # significance flags
    res["sig_raw"] = res["pval"] < alpha
    res["sig_fdr"] = res["p_fdr"] < alpha

    # stat_fdr: statistic retained only when FDR-significant
    # 注意：FDR 校正的是 p-value，不是 statistic 本身
    res["stat_fdr"] = np.where(
        res["sig_fdr"],
        res["stat"],
        np.nan
    )

    # cleaner column order
    cols = [
        "metric",
        "test_method",
        "comparison",
        "model_1",
        "model_2",
        "n_pairs",
        "model_1_mean",
        "model_2_mean",
        "mean_diff_1_minus_2",
        "std_diff",
        "stat_name",
        "stat",
        "pval",
        "p_fdr",
        "stat_fdr",
        "sig_raw",
        "sig_fdr",
    ]

    res = res[cols]

    if display_result:
        display(res)

    return res