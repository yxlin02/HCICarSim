import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

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

def plot_demographics(
    df,
    categorical_cols=None,
    continuous_cols=None,
    figsize_cat=(12, 6),
    figsize_cont=(2, 3),
):
    if categorical_cols is None:
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

    if continuous_cols is None:
        continuous_cols = [
            "age",
            "driving experience",
        ]

    # ========= categorical: pie charts =========
    cat_cols_use = [col for col in categorical_cols if col in df.columns]
    n_cat = len(cat_cols_use)

    if n_cat > 0:
        ncols = 4
        nrows = int(np.ceil(n_cat / ncols))
        fig, axes = plt.subplots(nrows, ncols, figsize=figsize_cat)
        axes = np.array(axes).reshape(-1)

        for i, col in enumerate(cat_cols_use):
            ax = axes[i]

            x = df[col].astype("string").str.strip().str.lower()
            counts = x.value_counts(dropna=False)

            labels = counts.index.astype(str)
            sizes = counts.values

            ax.pie(
                sizes,
                labels=labels,
                autopct="%1.1f%%",
                startangle=90,
            )
            ax.set_title(col)

        for j in range(i + 1, len(axes)):
            axes[j].axis("off")

        # fig.suptitle("Demographic Categorical Variables", fontsize=14)
        fig.tight_layout()
        plt.show()

    # ========= continuous: boxplots =========
    cont_cols_use = [col for col in continuous_cols if col in df.columns]

    for col in cont_cols_use:
        x = pd.to_numeric(df[col], errors="coerce").dropna()

        if len(x) == 0:
            continue

        plt.figure(figsize=figsize_cont)
        plt.boxplot(x, vert=True)
        # plt.ylabel(col)
        plt.title(f"{col}")
        plt.xticks([1], [col])
        plt.show()