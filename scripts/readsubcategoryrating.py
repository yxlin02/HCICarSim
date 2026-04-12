import re
import pandas as pd

def format(df_subcategory_rating):

    df_subcategory_rating = df_subcategory_rating[
        [col for col in df_subcategory_rating.columns if col.startswith("Q")]
    ]

    df_subcategory_rating = df_subcategory_rating.rename(columns={
        "Q1.1": "sub_id",
        "Q1.2": "mode",
        "Q1.3": "block_id"
    })

    df_subcategory_rating = (
        df_subcategory_rating.iloc[2:]
        .reset_index(drop=True)
        .apply(pd.to_numeric, errors="coerce")
        .astype("Int16")
    )

    rating_cols = [
        col for col in df_subcategory_rating.columns
        if re.match(r"^Q\d+\.[234]$", col)
    ]

    df_long = df_subcategory_rating.melt(
        id_vars=["sub_id", "mode", "block_id"],
        value_vars=rating_cols,
        var_name="question",
        value_name="rating"
    )

    df_long[["q_num", "score_type"]] = df_long["question"].str.extract(r"^Q(\d+)\.(\d)$")

    df_long["q_num"] = df_long["q_num"].astype(int)
    df_long["score_type"] = df_long["score_type"].astype(int)

    df_long["trial_id"] = df_long["q_num"] - 1

    score_map = {
        2: "appropriateness",
        3: "disturbance",
        4: "satisfaction"
    }
    df_long["score_type"] = df_long["score_type"].map(score_map)

    df_long = df_long.pivot_table(
        index=["sub_id", "mode", "block_id", "trial_id"],
        columns="score_type",
        values="rating",
        aggfunc="first"
    ).reset_index()

    df_long.columns.name = None

    df_long = df_long[
        ["sub_id", "mode", "block_id", "trial_id",
        "appropriateness", "disturbance", "satisfaction"]
    ]

    assert len(df_long["trial_id"].unique()) == 10

    return df_long

def format_prior(df_subject_prior):
    SUB_NUM_COL = ["Q3",]
    SUBCATEGORY_COLS = [ "101", "102", "103", "104", "105", "201", "202", "203", "204", "205",]

    df_subject_prior = df_subject_prior[
        [col for col in df_subject_prior.columns if col in SUBCATEGORY_COLS + SUB_NUM_COL]
    ]

    assert len(df_subject_prior.columns) == len(SUBCATEGORY_COLS + SUB_NUM_COL), "missing columns in df!!!"

    df_subject_prior = df_subject_prior.rename(columns={
        "Q3": "sub_id",
    })

    df_subject_prior = (
        df_subject_prior.iloc[2:]
        .reset_index(drop=True)
        .apply(pd.to_numeric, errors="coerce")
        .astype("Int16")
    )

    for col in SUBCATEGORY_COLS:
        df_subject_prior[col] /= 100.0
    
    return df_subject_prior

def format_prior(df_subject_prior):
    SUB_NUM_COL = ["Q3",]
    SUBCATEGORY_COLS = [ "101", "102", "103", "104", "105", "201", "202", "203", "204", "205",]

    df_subject_prior = df_subject_prior[
        [col for col in df_subject_prior.columns if col in SUBCATEGORY_COLS + SUB_NUM_COL]
    ]

    assert len(df_subject_prior.columns) == len(SUBCATEGORY_COLS + SUB_NUM_COL), "missing columns in df!!!"

    df_subject_prior = df_subject_prior.rename(columns={
        "Q3": "sub_id",
    })

    df_subject_prior = (
        df_subject_prior.iloc[2:]
        .reset_index(drop=True)
        .apply(pd.to_numeric, errors="coerce")
        .astype("Int16")
    )

    for col in SUBCATEGORY_COLS:
        df_subject_prior[col] /= 100.0
    
    return df_subject_prior

def format_demographics(df_demographics):
    COL_DICT = {
        "Q1": "age",
        "Q2": "gender",
        "Q3": "nationality",
        "Q4": "georgia tech faculty",
        "Q5": "driver license",
        "Q6": "education level",
        "Q7": "driving frequency",
        "Q8": "driving experience",
        "Q9": "in-car ai agent experience",
        "Q11": "dominant hand",
        "Q12": "sub_id",
    }

    df_demographics = df_demographics[
        [col for col in df_demographics.columns if col in COL_DICT.keys()]
    ]

    df_demographics = df_demographics.rename(columns=COL_DICT)

    df_demographics = (
        df_demographics.iloc[2:]
        .reset_index(drop=True)
    )

    return df_demographics