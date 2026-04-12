import pandas as pd
import numpy as np

def summarize(df):

    results = {}

    # ========= continuous =========
    continuous_cols = [
        "driving experience",
        "age",
    ]

    for col in continuous_cols:
        if col in df.columns:
            x = pd.to_numeric(df[col], errors="coerce").dropna()

            results[col] = {
                "type": "continuous",
                "mean": x.mean(),
                "median": x.median(),
                "std": x.std(),
                "n": len(x),
            }

    # ========= categorical =========
    categorical_cols = [
        "gender",
        "nationality",
        "georgia tech faculty",
        "driver license",
        "education level",
        "driving frequency",
        "in-car ai agent experience",
        "dominant hand",
    ]

    for col in categorical_cols:
        if col in df.columns:

            x = df[col].astype(str).str.strip().str.lower()

            counts = x.value_counts(dropna=False)
            ratios = counts / len(df)

            results[col] = {
                "type": "categorical",
                "distribution": pd.DataFrame({
                    "count": counts,
                    "ratio": ratios
                })
            }

    return results

def print_report(results):
    for col, res in results.items():
        print(f"\n=== {col} ===")

        if res["type"] == "continuous":
            print(f"mean   : {res['mean']:.3f}")
            print(f"median : {res['median']:.3f}")
            print(f"std    : {res['std']:.3f}")
            print(f"n      : {res['n']}")

        elif res["type"] == "categorical":
            df_dist = res["distribution"]
            print(df_dist.to_string())