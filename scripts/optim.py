import numpy as np
import pandas as pd
from scipy.optimize import differential_evolution, minimize
from sklearn.metrics import accuracy_score


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