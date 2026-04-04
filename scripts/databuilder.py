import numpy as np
import pandas as pd
from collections import defaultdict

SUBJECT_TYPE_PRIOR_DICT = {
    (1, 1): 0.866667,
    (1, 2): 0.866667,
    (2, 1): 0.666667,
    (2, 2): 0.533333,
    (3, 1): 0.600000,
    (3, 2): 0.533333,
    (4, 1): 0.866667,
    (4, 2): 0.266667,
    (5, 1): 0.733333,
    (5, 2): 0.400000,
    (6, 1): 0.800000,
    (6, 2): 0.266667,
    (7, 1): 0.800000,
    (7, 2): 0.666667,
    (8, 1): 0.800000,
    (8, 2): 0.600000,
    (9, 1): 0.733333,
    (9, 2): 0.666667,
    (10, 1): 0.933333,
    (10, 2): 0.533333,
    (11, 1): 0.800000,
    (11, 2): 0.666667,
    (12, 1): 0.857143,
    (12, 2): 0.928571,
}

def _safe_mean(x):
    if len(x) == 0:
        return np.nan
    return float(np.mean(x))


def _safe_var(x):
    if len(x) == 0:
        return np.nan
    return float(np.var(x, ddof=0))


def _zscore_with_ref(x, ref_mean, ref_std):
    if pd.isna(ref_std) or ref_std == 0:
        return np.full(len(x), np.nan)
    return (x - ref_mean) / ref_std


def _find_time_col(df):
    """
    Try to find the best timestamp column in PawnCar.csv.
    Prefer _time_ms, otherwise fallback to gametime_ms.
    """
    if "_time_ms" in df.columns:
        # print("Using _time_ms as time column for PawnCar.csv")
        return "_time_ms"
    elif "gametime_ms" in df.columns:
        return "gametime_ms"
    else:
        raise ValueError("PawnCar.csv must contain either '_time_ms' or 'gametime_ms'.")


def _extract_pattern_info(pattern_id):
    """
    pattern_id / current_encode_id assumed to be 5-digit int-like.
    Returns:
        recommendation: int
        category: int (1 digit)
        subcategory: int (3 digits; here use last 3 digits)
    """
    s = str(int(pattern_id)).zfill(5)
    recommendation = int(s)
    category = int(s[0])
    subcategory = int(s[:3])
    return recommendation, category, subcategory


def _get_window_slice(df_car, time_col, center_t, start_offset_ms, end_offset_ms):
    """
    Window: [center_t + start_offset_ms, center_t + end_offset_ms]
    """
    # print(f"Finding window slice in df_car with {len(df_car)} rows, time_col={time_col}")
    # print("df_car time range: ", df_car[time_col].min(), " to ", df_car[time_col].max())
    t0 = center_t + start_offset_ms
    t1 = center_t + end_offset_ms
    # print(f"Getting window slice: center={center_t} ms, start_offset={t0} ms, end_offset={t1} ms")
    return df_car[(df_car[time_col] >= t0) & (df_car[time_col] <= t1)].copy()


def build_per_reaction_df(
    data_dict,
    pre_window_ms=2000,
    post_window_ms=5000,
    default_prior_accept_prob=0.5,
    default_eval_value=np.nan,
):
    """
    Build per-reaction dataframe from nested data_dict.

    Required:
    - trial["marker"] contains:
        _time_ms, gametime_ms, scene_id, block_id, trial_id, current_encode_id, marker
    - trial["car"] contains:
        throttle_input, steering_input, and timestamp column
          (_time_ms preferred, otherwise gametime_ms)

    Returns:
        pd.DataFrame, one row per reaction event
    """
    records = []

    for sub_key, scenes in data_dict.items():
        sub = int(sub_key.split("_")[1])
        mode = "auto" if sub % 2 == 0 else "manual"

        for scene_key, trial in scenes.items():
            df_marker = trial["marker"].copy()
            df_car = trial["car"].copy()
            df_subcategory_rating = trial["subcategory_rating"].copy()

            if len(df_marker) == 0 or len(df_car) == 0:
                continue

            # ---------- basic checks ----------
            required_marker_cols = {
                "_time_ms", "gametime_ms",
                "trial_id", "current_encode_id", "marker"
            }
            missing_marker = required_marker_cols - set(df_marker.columns)
            if missing_marker:
                raise ValueError(
                    f"{sub_key}-{scene_key}: marker is missing columns {missing_marker}"
                )

            # required_car_cols = {"steering_input", "throttle_input"}
            required_car_cols = {"throttle_input",}
            missing_car = required_car_cols - set(df_car.columns)
            if missing_car:
                raise ValueError(
                    f"{sub_key}-{scene_key}: car is missing columns {missing_car}"
                )

            car_time_col = _find_time_col(df_car)

            # sort by time
            df_marker = df_marker.sort_values("_time_ms")
            df_car = df_car.sort_values("_time_ms")
            # print(df_car[[car_time_col]].describe())

            # ---------- prior over whole subject-scene-mode ----------
            # steering_mean_all = df_car["steering_input"].mean()
            # steering_std_all = df_car["steering_input"].std(ddof=0)
            # steering_var_all = df_car["steering_input"].var(ddof=0)

            throttle_mean_all = df_car["throttle_input"].mean()
            throttle_std_all = df_car["throttle_input"].std(ddof=0)
            throttle_var_all = df_car["throttle_input"].var(ddof=0)

            # ---------- reaction rows ----------
            df_react = df_marker[df_marker["marker"].str.startswith("reaction_")].copy()
            if len(df_react) == 0:
                continue

            for idx, react_row in df_react.iterrows():
                reaction_marker = react_row["marker"]               # e.g. reaction_accept
                reaction_label = reaction_marker.replace("reaction_", "")
                accept_bool = (reaction_label == "accept")

                reaction_time_unix = react_row["_time_ms"]
                reaction_time_game = react_row["gametime_ms"]
                trial_id = react_row["trial_id"]
                pattern_id = react_row["current_encode_id"]

                recommendation, rec_category, rec_subcategory = _extract_pattern_info(pattern_id)

                # ---------- find matched evaluation ----------
                subset = df_subcategory_rating[
                    df_subcategory_rating["trial_id"] == trial_id
                ]

                assert len(subset) == 1, f"Expected 1 row, got {len(subset)}"

                appropriateness = subset["appropriateness"].iloc[0] if "appropriateness" in subset.columns else default_eval_value
                disturbance = subset["disturbance"].iloc[0] if "disturbance" in subset.columns else default_eval_value
                satisfaction = subset["satisfaction"].iloc[0] if "satisfaction" in subset.columns else default_eval_value


                # ---------- find matched trigger ----------
                # same trial + same encode + before this reaction
                trigger_candidates = df_marker[
                    (df_marker["trial_id"] == trial_id) &
                    (df_marker["current_encode_id"] == pattern_id) &
                    (df_marker["marker"] == "trigger") &
                    (df_marker["_time_ms"] <= reaction_time_unix)
                ].copy()

                if len(trigger_candidates) == 0:
                    # fallback: same trial only
                    trigger_candidates = df_marker[
                        (df_marker["trial_id"] == trial_id) &
                        (df_marker["marker"] == "trigger") &
                        (df_marker["_time_ms"] <= reaction_time_unix)
                    ].copy()

                if len(trigger_candidates) == 0:
                    trigger_time_unix = np.nan
                    reaction_time_ms = np.nan
                else:
                    trigger_row = trigger_candidates.sort_values("_time_ms").iloc[-1]
                    trigger_time_unix = trigger_row["_time_ms"]
                    reaction_time_ms = reaction_time_unix - trigger_time_unix
                    # print(f"{sub_key}-{scene_key}-trial{trial_id}: found trigger for reaction at {trigger_time_unix} ms")

                # ---------- windows around reaction ----------
                df_pre = _get_window_slice(
                    df_car, car_time_col,
                    center_t=trigger_time_unix if car_time_col == "_time_ms" else reaction_time_game,
                    start_offset_ms=-pre_window_ms,
                    end_offset_ms=0
                )

                df_post = _get_window_slice(
                    df_car, car_time_col,
                    center_t=trigger_time_unix if car_time_col == "_time_ms" else reaction_time_game,
                    start_offset_ms=0,
                    end_offset_ms=post_window_ms
                )

                # ---------- scenario window zscores ----------
                if len(df_pre) > 0:
                    # pre_steer_z = _zscore_with_ref(
                    #     df_pre["steering_input"].to_numpy(),
                    #     steering_mean_all,
                    #     steering_std_all
                    # )
                    pre_throttle_z = _zscore_with_ref(
                        df_pre["throttle_input"].to_numpy(),
                        throttle_mean_all,
                        throttle_std_all
                    )
                    # mean_steering_z_pre2s = _safe_mean(pre_steer_z)
                    mean_throttle_z_pre2s = _safe_mean(pre_throttle_z)
                    mean_throttle_pre2s = df_pre["throttle_input"].mean()
                    var_throttle_pre2s = _safe_var(df_pre["throttle_input"])
                else:
                    # mean_steering_z_pre2s = np.nan
                    mean_throttle_z_pre2s = np.nan
                    print(f"{sub_key}-{scene_key}-trial{trial_id}: no data in pre-reaction window")
                    # mean_throttle_pre2s = np.nan

                # ---------- reaction window zscores ----------
                if len(df_post) > 0:
                    # post_steer_z = _zscore_with_ref(
                    #     df_post["steering_input"].to_numpy(),
                    #     steering_mean_all,
                    #     steering_std_all
                    # )
                    post_throttle_z = _zscore_with_ref(
                        df_post["throttle_input"].to_numpy(),
                        throttle_mean_all,
                        throttle_std_all
                    )
                    # mean_steering_z_post5s = _safe_mean(post_steer_z)
                    mean_throttle_z_post5s = _safe_mean(post_throttle_z)
                    mean_throttle_post5s = df_post["throttle_input"].mean()
                    var_throttle_post5s = _safe_var(df_post["throttle_input"])
                else:
                    # mean_steering_z_post5s = np.nan
                    mean_throttle_z_post5s = np.nan
                    mean_throttle_post5s = np.nan
                    var_throttle_post5s = np.nan


                # ---------- pattern-level scenario features ----------
                # default NaN first
                intensity = 0.5
                coherence = 0.0

                df_pattern = trial.get("pattern", None)
                if df_pattern is not None and len(df_pattern) > 0:
                    if "intensity" in df_pattern.columns:
                        intensity = df_pattern["intensity"].iloc[0]
                    elif "Intensity" in df_pattern.columns:
                        intensity = df_pattern["Intensity"].iloc[0]

                    if "coherence" in df_pattern.columns:
                        coherence = df_pattern["coherence"].iloc[0]
                    elif "Coherence" in df_pattern.columns:
                        coherence = df_pattern["Coherence"].iloc[0]
                
                car_density_map = {
                    1: 0.2,
                    2: 0.8,
                    3: 0.8,
                    4: 0.2,
                    5: 0.8,
                    6: 0.8,
                }

                time_pressure_map = {
                    1: 0,
                    2: 0,
                    3: 1,
                    4: 0,
                    5: 0,
                    6: 1,
                }

                subject_prior_accept_prob_subcategory = SUBJECT_TYPE_PRIOR_DICT.get(
                    (sub, rec_category),
                    default_prior_accept_prob
                )

                records.append({
                    # ---------------- meta ----------------
                    "sub_id": sub,
                    "scene_id": scene_key,
                    "mode": mode,
                    "trial_id": trial_id,
                    "recommendation": recommendation,
                    "recommendation_category": rec_category,
                    "recommendation_subcategory": rec_subcategory,

                    # ---------------- prior ----------------
                    "subject_prior_accept_prob_subcategory": subject_prior_accept_prob_subcategory if not pd.isna(subject_prior_accept_prob_subcategory) else default_prior_accept_prob,
                    # "prior_mean_steering_input": float(steering_mean_all),
                    # "prior_var_steering_input": float(steering_var_all),
                    "prior_mean_throttle_input": float(throttle_mean_all),
                    "prior_var_throttle_input": float(throttle_var_all),

                    # ---------------- scenario ----------------
                    "intensity": intensity,
                    "coherence": coherence,
                    # "mean_steering_input_zscore_pre2s": mean_steering_z_pre2s,
                    "mean_throttle_input_zscore_pre2s": mean_throttle_z_pre2s,
                    "mean_throttle_pre2s": mean_throttle_pre2s,
                    "var_throttle_pre2s": var_throttle_pre2s,
                    "car_density": car_density_map[int(scene_key.split("_")[1])],
                    "time_pressure": time_pressure_map[int(scene_key.split("_")[1])],

                    # ---------------- reaction ----------------
                    "reaction": reaction_label,
                    "accept": bool(accept_bool),
                    "reaction_time_ms": reaction_time_ms,
                    # "mean_steering_input_zscore_post5s": mean_steering_z_post5s,
                    "mean_throttle_input_zscore_post5s": mean_throttle_z_post5s,
                    "mean_throttle_post5s": mean_throttle_post5s,
                    "var_throttle_post5s": var_throttle_post5s,

                    # ---------------- evaluation ----------------
                    "appropriateness": appropriateness,
                    "disturbance": disturbance,
                    "satisfaction": satisfaction,

                    # ---------------- bookkeeping ----------------
                    "reaction__time_ms": reaction_time_unix,
                    "trigger__time_ms": trigger_time_unix,
                })

    return pd.DataFrame(records)

def build_per_reaction_data_dict(df_reaction):
    """
    Nested dict:
    data[sub_id][scene_id] = list of reaction dicts
    """
    out = defaultdict(lambda: defaultdict(list))

    for _, row in df_reaction.iterrows():
        sub_key = f"sub_{int(row['sub_id'])}"
        scene_key = str(row["scene_id"])

        item = {
            "meta": {
                "sub_id": int(row["sub_id"]),
                "scene_id": row["scene_id"],
                "mode": row["mode"],
                "trial_id": int(row["trial_id"]) if not pd.isna(row["trial_id"]) else None,
                "recommendation": int(row["recommendation"]),
                "recommendation_category": int(row["recommendation_category"]),
                "recommendation_subcategory": int(row["recommendation_subcategory"]),
            },
            "prior": {
                "subject_prior_accept_prob_subcategory": float(row["subject_prior_accept_prob_subcategory"]),
                # "mean_steering_input": float(row["prior_mean_steering_input"]) if not pd.isna(row["prior_mean_steering_input"]) else np.nan,
                # "var_steering_input": float(row["prior_var_steering_input"]) if not pd.isna(row["prior_var_steering_input"]) else np.nan,
                "mean_throttle_input": float(row["prior_mean_throttle_input"]) if not pd.isna(row["prior_mean_throttle_input"]) else np.nan,
                "var_throttle_input": float(row["prior_var_throttle_input"]) if not pd.isna(row["prior_var_throttle_input"]) else np.nan,
            },
            "scenario": {
                "intensity": float(row["intensity"]) if not pd.isna(row["intensity"]) else np.nan,
                "coherence": float(row["coherence"]) if not pd.isna(row["coherence"]) else np.nan,
                # "mean_steering_input_zscore_pre2s": float(row["mean_steering_input_zscore_pre2s"]) if not pd.isna(row["mean_steering_input_zscore_pre2s"]) else np.nan,
                "mean_throttle_input_zscore_pre2s": float(row["mean_throttle_input_zscore_pre2s"]) if not pd.isna(row["mean_throttle_input_zscore_pre2s"]) else np.nan,
            },
            "reaction": {
                "reaction": row["reaction"],
                "accept": bool(row["accept"]),
                "reaction_time_ms": float(row["reaction_time_ms"]) if not pd.isna(row["reaction_time_ms"]) else np.nan,
                # "mean_steering_input_zscore_post5s": float(row["mean_steering_input_zscore_post5s"]) if not pd.isna(row["mean_steering_input_zscore_post5s"]) else np.nan,
                "mean_throttle_input_zscore_post5s": float(row["mean_throttle_input_zscore_post5s"]) if not pd.isna(row["mean_throttle_input_zscore_post5s"]) else np.nan,
            },
            "evaluation": {
                "disturbance": float(row["disturbance"]) if not pd.isna(row["disturbance"]) else np.nan,
                "satisfaction": float(row["satisfaction"]) if not pd.isna(row["satisfaction"]) else np.nan,
                "appropriateness": float(row["appropriateness"]) if not pd.isna(row["appropriateness"]) else np.nan,
            }
        }

        out[sub_key][scene_key].append(item)

    return out