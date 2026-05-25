import numpy as np
import pandas as pd

def coef_ci_table(result):
    params = result.params
    ci = result.conf_int()
    out = pd.DataFrame({
        "coef": params,
        "ci_low": ci[0],
        "ci_high": ci[1],
        "p": result.pvalues
    })
    return out

def test_effect(result, terms):
    print("Testing:", terms)
    print(result.wald_test(terms))


def sklearn_cv_results_to_dynamics_format(
    cv_results,
    reference_df=None,
    model_name="logistic",
):
    """
    Convert sklearn cross_validate output dict to the same fold-level format
    as 1d/2d dynamical model CV results.

    Parameters
    ----------
    cv_results : dict
        sklearn cross_validate result dictionary.

    reference_df : pd.DataFrame or None
        Example: cv_1d_results.
        If provided, repeat/fold/n_valid/n_subjects_test will be copied from it.

    model_name : str
        Optional model label.

    Returns
    -------
    df_out : pd.DataFrame
        Fold-level CV dataframe.
    """

    n_folds = len(cv_results["test_accuracy"])

    if reference_df is not None:
        df_out = reference_df[[
            "repeat", "fold", "n_valid", "n_subjects_test"
        ]].copy()
    else:
        df_out = pd.DataFrame({
            "repeat": np.ones(n_folds, dtype=int),
            "fold": np.arange(1, n_folds + 1),
            "n_valid": np.nan,
            "n_subjects_test": np.nan,
        })

    # train metrics are unavailable unless return_train_score=True
    df_out["train_acc"] = cv_results.get("train_accuracy", np.full(n_folds, np.nan))
    df_out["train_bal_acc"] = cv_results.get("train_balanced_accuracy", np.full(n_folds, np.nan))
    df_out["train_auc"] = cv_results.get("train_roc_auc", np.full(n_folds, np.nan))
    df_out["train_logloss"] = cv_results.get("train_neg_log_loss", np.full(n_folds, np.nan))
    df_out["train_score"] = np.nan

    # test metrics
    df_out["test_acc"] = cv_results["test_accuracy"]
    df_out["test_bal_acc"] = cv_results["test_balanced_accuracy"]
    df_out["test_auc"] = cv_results["test_roc_auc"]

    # optional metrics if available
    df_out["test_logloss"] = cv_results.get("test_neg_log_loss", np.full(n_folds, np.nan))
    df_out["test_score"] = np.nan

    # these are not available from standard cross_validate output
    df_out["test_accept_rate"] = np.nan
    df_out["test_pred_accept_rate"] = np.nan
    df_out["test_rate_error"] = np.nan

    df_out["model"] = model_name

    return df_out


def make_chance_cv_results(reference_df, acc=0.69, model_name="chance"):
    """
    Make chance baseline dataframe with the same fold-level format.
    Accuracy is set to acc, while balanced accuracy and AUC are set to 0.5.
    """

    df_out = reference_df[[
        "repeat", "fold", "n_valid", "n_subjects_test"
    ]].copy()

    df_out["train_acc"] = acc
    df_out["train_bal_acc"] = 0.5
    df_out["train_auc"] = 0.5
    df_out["train_logloss"] = np.nan
    df_out["train_score"] = 0.5

    df_out["test_acc"] = acc
    df_out["test_bal_acc"] = 0.5
    df_out["test_auc"] = 0.5
    df_out["test_logloss"] = np.nan
    df_out["test_score"] = 0.5

    df_out["test_accept_rate"] = acc
    df_out["test_pred_accept_rate"] = acc
    df_out["test_rate_error"] = 0.0

    df_out["model"] = model_name

    return df_out