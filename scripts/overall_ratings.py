import pandas as pd

# ===========================
# NASA-TLX
# ===========================

def tlx_format(csv_path):

    df = pd.read_csv(csv_path)

    # ---------- 只保留需要的列 ----------
    tlx_q_cols = [f"Q1_{i}" for i in range(1, 7)]
    meta_cols = ["Q2", "Q4"]  # sub_id, version

    keep_cols = [c for c in tlx_q_cols + meta_cols if c in df.columns]
    df = df[keep_cols].copy()

    # ---------- 删除前两行 ----------
    df = df.iloc[2:].reset_index(drop=True)

    # ---------- 转数值 ----------
    for c in tlx_q_cols:
        df[c] = (
            df[c]
            .astype(str)
            .str.strip()          # 去空格
            .replace("", None)    # 空字符串 → NaN
        )
        df[c] = pd.to_numeric(df[c], errors="coerce")

    # ---------- 重命名 ----------
    rename_map = {
        "Q1_1": "mental_demand",
        "Q1_2": "physical_demand",
        "Q1_3": "temporal_demand",
        "Q1_4": "performance",
        "Q1_5": "effort",
        "Q1_6": "frustration",
        "Q2": "sub_id",
        "Q4": "version",
    }
    df = df.rename(columns=rename_map)

    # ---------- Performance reverse ----------
    df["performance_rev"] = 22 - df["performance"]

    # ---------- overall（只用 TLX 六维） ----------
    tlx_cols = [
        "mental_demand",
        "physical_demand",
        "temporal_demand",
        "performance_rev",
        "effort",
        "frustration",
    ]

    df["tlx_overall"] = df[tlx_cols].mean(axis=1)

    return df


def test_version_effect(df, metric="tlx_overall", multitest_method="fdr_bh"):
    from itertools import combinations
    from scipy.stats import ttest_rel
    from statsmodels.stats.multitest import multipletests

    pivot = df.pivot_table(
        index="sub_id",
        columns="version",
        values=metric
    )

    pivot = pivot.dropna()

    versions = pivot.columns

    results = []
    pvals = []

    for v1, v2 in combinations(versions, 2):
        t, p = ttest_rel(pivot[v1], pivot[v2])
        results.append({"v1": v1, "v2": v2, "t": t, "p": p})
        pvals.append(p)

    _, pvals_fdr, _, _ = multipletests(pvals, method=multitest_method)

    for i in range(len(results)):
        results[i]["p_fdr"] = pvals_fdr[i]

    return pd.DataFrame(results)

import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

def plot_tlx_by_version(df,  metrics=None, rotation=30, title="TLX", figsize=(6,4)):

    if metrics is None:
        metrics = [
            "mental_demand",
            "physical_demand",
            "temporal_demand",
            "performance_rev",
            "effort",
            "frustration",
            "tlx_overall",
        ]

    # ---------- 转 long format ----------
    df_long = df.melt(
        id_vars=["version"],
        value_vars=metrics,
        var_name="metric",
        value_name="value"
    )

    # ---------- 画图 ----------
    plt.figure(figsize=figsize)

    sns.barplot(
        data=df_long,
        x="metric",
        y="value",
        hue="version",
        edgecolor="black",
    )

    plt.xticks(rotation=rotation, ha="right")
    plt.xlabel("")
    plt.title(f"{title} metrics by version")
    plt.tight_layout()
    plt.legend(bbox_to_anchor=(1.0,0), loc="lower left", frameon=False)

    plt.show()


# ===========================
# SUS
# ===========================
def sus_format(csv_path):
    """
    处理 SUS 数据：
    1. 提取 SUS 题目列
    2. 重命名
    3. 计算 SUS 0–100 分
    4. 保留 sub_id / version
    """

    df = pd.read_csv(csv_path)

    # ---------- 保留需要的列 ----------
    sus_cols = ["Q2","Q3","Q4","Q5","Q6","Q7","Q11","Q14","Q13","Q12"]
    meta_cols = ["Q9","Q15"]

    keep_cols = [c for c in sus_cols + meta_cols if c in df.columns]
    df = df[keep_cols].copy()

    # ---------- 删除前两行（如果有） ----------
    df = df.iloc[2:].reset_index(drop=True)

    # ---------- 转数值 ----------
    for c in sus_cols:
        df[c] = pd.to_numeric(df[c], errors="coerce")

    # ---------- 重命名 ----------
    rename_map = {
        "Q2": "sus1",
        "Q3": "sus2",
        "Q4": "sus3",
        "Q5": "sus4",
        "Q6": "sus5",
        "Q7": "sus6",
        "Q11": "sus7",
        "Q14": "sus8",
        "Q13": "sus9",
        "Q12": "sus10",
        "Q9": "sub_id",
        "Q15": "version",
    }
    df = df.rename(columns=rename_map)

    # ---------- SUS 计算 ----------
    odd_cols = ["sus1","sus3","sus5","sus7","sus9"]
    even_cols = ["sus2","sus4","sus6","sus8","sus10"]

    # 正向题
    odd_score = df[odd_cols].apply(lambda x: x - 1)

    # 反向题
    even_score = df[even_cols].apply(lambda x: 5 - x)

    # 总分
    df["sus_score"] = (odd_score.sum(axis=1) + even_score.sum(axis=1)) * 2.5

    return df