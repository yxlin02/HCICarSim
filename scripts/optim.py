import numpy as np
import pandas as pd
from scipy.optimize import differential_evolution, minimize
from sklearn.metrics import accuracy_score

from scripts import twoddynamics, oneddynamics, databuilder, readsubcategoryrating, intensity_calculation, demographics, plotter, optim, machinelearning, overall_ratings, dynamics_evaluation


def objective(df_fit, vec, model, fixed_params, param_names):
    try:
        params = dict(fixed_params)
        for k, v in zip(param_names, vec):
            params[k] = float(v)

        df_sim = model(df_fit, **params)

        if "accept_true" not in df_sim.columns:
            return 1e6
        if "accept_pred" not in df_sim.columns:
            return 1e6

        valid = df_sim.dropna(subset=["accept_true", "accept_pred"]).copy()
        if len(valid) == 0:
            return 1e6

        y_true = valid["accept_true"].astype(int).values
        y_pred = valid["accept_pred"].astype(int).values

        acc = accuracy_score(y_true, y_pred)

        loss = -acc
        return float(loss)

    except Exception as e:
        print("objective failed:", e)
        return 1e6


class EarlyStopper:
    def __init__(self, patience=5, min_delta=1e-4, objective=None):
        self.patience = patience
        self.min_delta = min_delta
        self.best = np.inf
        self.counter = 0
        self.objective = objective

    def __call__(self, xk, convergence):
        current = self.objective(xk)

        if current < self.best - self.min_delta:
            self.best = current
            self.counter = 0
        else:
            self.counter += 1

        print(f"[EarlyStop] best={self.best:.4f}, current={current:.4f}, no_improve={self.counter}")

        if self.counter >= self.patience:
            print("Early stopping triggered.")
            return True

        return False
    

# =========================
# repeated group k-fold helper
# =========================
def repeated_group_kfold_indices(
    df,
    group_col="sub_id",
    n_splits=5,
    n_repeats=5,
    random_state=42
):
    """
    Repeated GroupKFold-like split.

    Each repeat randomly shuffles unique subjects, then splits subjects into
    n_splits folds. This ensures that the same subject never appears in both
    training and test sets within one fold.
    """
    rng = np.random.default_rng(random_state)
    unique_groups = np.array(df[group_col].dropna().unique())

    for repeat in range(1, n_repeats + 1):
        shuffled_groups = unique_groups.copy()
        rng.shuffle(shuffled_groups)

        group_folds = np.array_split(shuffled_groups, n_splits)

        for fold, test_groups in enumerate(group_folds, start=1):
            test_mask = df[group_col].isin(test_groups).values
            train_mask = ~test_mask

            train_idx = np.where(train_mask)[0]
            test_idx = np.where(test_mask)[0]

            yield repeat, fold, train_idx, test_idx, test_groups


from sklearn.metrics import accuracy_score, roc_auc_score, log_loss
import numpy as np
import pandas as pd


def evaluate_dynamic_params(
    df,
    params,
    simulator_func,
    metric_weights=None,
    min_valid=20,
):
    if metric_weights is None:
        metric_weights = {
            "auc": 0.6,
            "acc": 0.3,
            "rate_error": 0.1,
            "logloss": 0.0,
        }

    df_sim = simulator_func(
        df,
        **params
    )

    valid = df_sim.dropna(
        subset=["accept_true", "accept_pred", "p_accept_pred"]
    ).copy()

    if len(valid) < min_valid:
        return {
            "score": -np.inf,
            "acc": np.nan,
            "auc": np.nan,
            "logloss": np.nan,
            "accept_rate": np.nan,
            "pred_accept_rate": np.nan,
            "rate_error": np.nan,
            "n_valid": len(valid),
        }

    y_true = valid["accept_true"].astype(int).values
    y_pred = valid["accept_pred"].astype(int).values

    y_prob = np.clip(
        valid["p_accept_pred"].astype(float).values,
        1e-6,
        1 - 1e-6
    )

    acc = accuracy_score(y_true, y_pred)

    if len(np.unique(y_true)) < 2:
        auc = np.nan
    else:
        auc = roc_auc_score(y_true, y_prob)

    ll = log_loss(y_true, y_prob)

    accept_rate = np.mean(y_true)
    pred_accept_rate = np.mean(y_pred)
    rate_error = abs(pred_accept_rate - accept_rate)

    if np.isnan(auc):
        score = -np.inf
    else:
        score = (
            metric_weights.get("auc", 0.0) * auc
            + metric_weights.get("acc", 0.0) * acc
            - metric_weights.get("logloss", 0.0) * ll
            - metric_weights.get("rate_error", 0.0) * rate_error
        )

    return {
        "score": score,
        "acc": acc,
        "auc": auc,
        "logloss": ll,
        "accept_rate": accept_rate,
        "pred_accept_rate": pred_accept_rate,
        "rate_error": rate_error,
        "n_valid": len(valid),
    }

from scipy.optimize import differential_evolution


def optimize_2d_params(
    train_df,
    simulator_func,
    base_params,
    search_space,
    metric_weights=None,
    maxiter=50,
    popsize=10,
    random_state=42,
    polish=True,
):
    names = list(search_space.keys())
    bounds = [search_space[name] for name in names]

    def unpack(x):
        params = base_params.copy()
        for name, value in zip(names, x):
            params[name] = float(value)
        return params

    def objective(x):
        params = unpack(x)

        res = evaluate_dynamic_params(
            train_df,
            params,
            simulator_func,
            metric_weights=metric_weights,
        )

        if not np.isfinite(res["score"]):
            return 1e6

        return -res["score"]

    opt = differential_evolution(
        objective,
        bounds=bounds,
        maxiter=maxiter,
        popsize=popsize,
        tol=1e-3,
        polish=polish,
        seed=random_state,
        workers=1,
        updating="immediate",
    )

    best_params = unpack(opt.x)

    train_eval = evaluate_dynamic_params(
        train_df,
        best_params,
        simulator_func,
        metric_weights=metric_weights,
    )

    return {
        "best_params": best_params,
        "best_x": opt.x,
        "best_loss": opt.fun,
        "train_eval": train_eval,
        "success": opt.success,
        "message": opt.message,
    }

def random_search_dynamic_params(
    train_df,
    base_params,
    search_space,
    simulator_func,
    metric_weights=None,
    n_iter=100,
    random_state=42,
    patience=15,
    min_delta=1e-4,
    min_iter=20,
    verbose=True,
):
    rng = np.random.default_rng(random_state)
    names = list(search_space.keys())

    best_score = -np.inf
    best_params = None
    best_eval = None
    rows = []

    no_improve_count = 0
    stopped_early = False
    stop_iter = None

    for i in range(n_iter):
        params = base_params.copy()

        # sample parameters
        for name in names:
            low, high = search_space[name]
            params[name] = rng.uniform(low, high)

        # evaluate
        res = evaluate_dynamic_params(
            train_df,
            params,
            simulator_func=simulator_func,
            metric_weights=metric_weights,
        )

        row = {
            "iter": i,
            "score": res["score"],
            "acc": res["acc"],
            "auc": res["auc"],
            "logloss": res["logloss"],
            "accept_rate": res["accept_rate"],
            "pred_accept_rate": res["pred_accept_rate"],
            "rate_error": res["rate_error"],
        }
        row.update({name: params[name] for name in names})
        rows.append(row)

        score = res["score"]

        # check improvement
        if np.isfinite(score) and score > best_score + min_delta:
            best_score = score
            best_params = params.copy()
            best_eval = res.copy()
            no_improve_count = 0
        else:
            no_improve_count += 1

        # early stopping
        if i + 1 >= min_iter and no_improve_count >= patience:
            stopped_early = True
            stop_iter = i

            if verbose:
                print(
                    f"Early stopping at iter {i}: "
                    f"no improvement for {patience} iterations. "
                    f"Best score = {best_score:.4f}"
                )
            break

    return {
        "best_params": best_params,
        "train_eval": best_eval,
        "search_results": pd.DataFrame(rows),
        "stopped_early": stopped_early,
        "stop_iter": stop_iter,
        "best_score": best_score,
    }