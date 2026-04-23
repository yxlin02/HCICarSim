import math
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt


def plot_metric_by_group_across_subcategories(
    df,
    metric,
    group,
    style="boxplot",
    subcategory_col="recommendation_subcategory",
    col_wrap=5,
    figsize_per_ax=(3.2, 3.0),
    palette=None,
    sharey=True,
    order=None,
    subcategory_order=None,
    title=None,
    rotate_xticks=30,
):
    """
    对每个 subcategory 画出 metric 按 group 分类的图，
    所有 subcategory 结果放在同一张图上。

    Parameters
    ----------
    df : pd.DataFrame
    metric : str
        要画的指标列名
    group : str
        分组列名，例如 "version", "mode", "recommendation_category"
    style : str
        "boxplot" or "barplot"
    subcategory_col : str
        subcategory 列名，默认 "recommendation_subcategory"
    col_wrap : int
        每行放几个子图
    figsize_per_ax : tuple
        每个子图的大致尺寸 (width, height)
    palette : dict or list or None
        seaborn palette
    sharey : bool
        是否共享 y 轴
    order : list or None
        group 的显示顺序
    subcategory_order : list or None
        subcategory 的显示顺序
    title : str or None
        总标题
    rotate_xticks : int or float
        x 轴标签旋转角度
    """

    if metric not in df.columns:
        raise ValueError(f"`metric`={metric} not found in df.columns")
    if group not in df.columns:
        raise ValueError(f"`group`={group} not found in df.columns")
    if subcategory_col not in df.columns:
        raise ValueError(f"`subcategory_col`={subcategory_col} not found in df.columns")

    if style not in {"boxplot", "barplot"}:
        raise ValueError("`style` must be 'boxplot' or 'barplot'")

    df_plot = df[[subcategory_col, group, metric]].copy()
    df_plot = df_plot.dropna(subset=[subcategory_col, group, metric])

    if subcategory_order is None:
        if pd.api.types.is_categorical_dtype(df_plot[subcategory_col]):
            subcategory_order = list(df_plot[subcategory_col].cat.categories)
            subcategory_order = [x for x in subcategory_order if x in df_plot[subcategory_col].unique()]
        else:
            try:
                subcategory_order = sorted(df_plot[subcategory_col].unique())
            except Exception:
                subcategory_order = list(df_plot[subcategory_col].unique())

    n_subcats = len(subcategory_order)
    ncols = min(col_wrap, n_subcats)
    nrows = math.ceil(n_subcats / ncols)

    fig_w = figsize_per_ax[0] * ncols
    fig_h = figsize_per_ax[1] * nrows

    fig, axes = plt.subplots(
        nrows, ncols,
        figsize=(fig_w, fig_h),
        sharey=sharey
    )

    if isinstance(axes, np.ndarray):
        axes = axes.flatten()
    else:
        axes = [axes]

    legend_handles = None
    legend_labels = None

    for i, subcat in enumerate(subcategory_order):
        ax = axes[i]
        sub_df = df_plot[df_plot[subcategory_col] == subcat].copy()
        if group == "version":
            palette = {
                "default": "white",
                "personalized": "#A3E3C1",
                "personified": "#DEC1EC",
            }
        else:
            palette = palette

        if style == "boxplot":
            
            sns.boxplot(
                data=sub_df,
                x=group,
                y=metric,
                hue=group,
                order=order,
                palette=palette,
                ax=ax
            )

        elif style == "barplot":
            sns.barplot(
                data=sub_df,
                x=group,
                y=metric,
                hue=group,
                order=order,
                palette=palette,
                errorbar="se",
                edgecolor="black",
                ax=ax
            )

        sub_group_median = sub_df.groupby(group)[metric].median()

        # 获取 x 轴 label -> tick 的映射
        xticks = {label.get_text(): tick for tick, label in enumerate(ax.get_xticklabels())}

        # default 的位置和数值
        if "default" in xticks:
            x0 = xticks["default"]
            y0 = sub_group_median.get("default", np.nan)

            for g, x1 in xticks.items():
                if g == "default":
                    continue

                y1 = sub_group_median.get(g, np.nan)

                if np.isnan(y0) or np.isnan(y1):
                    continue
                ax.plot(
                    [x0, x1],
                    [y0, y1],
                    color="gray",
                    linewidth=1
                )
                ax.scatter([x0, x1], [y0, y1], color="gray", s=10, zorder=3)

        ax.set_title(f"{subcat}")
        ax.set_xlabel("")
        ax.set_ylabel("")
        # ax.set_ylabel(metric if (not sharey or i % ncols == 0) else "")
        ax.tick_params(axis="x", rotation=rotate_xticks, labelsize=8)

        if ax.get_legend() is not None:
            handles, labels = ax.get_legend_handles_labels()
            if legend_handles is None:
                legend_handles, legend_labels = handles, labels
            ax.get_legend().remove()

    # 删除多余空子图
    for j in range(n_subcats, len(axes)):
        fig.delaxes(axes[j])

    if title is None:
        title = f"{metric} by {group} across {subcategory_col}"

    fig.suptitle(title, fontsize=14)
    fig.tight_layout(rect=[0, 0, 1, 0.98])

    if legend_handles is not None:
        fig.legend(
            legend_handles,
            legend_labels,
            title=group,
            loc="center left",
            bbox_to_anchor=(1.01, 0.5),
            frameon=False
        )

    plt.show()