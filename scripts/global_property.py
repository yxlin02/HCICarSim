import pandas as pd
import numpy as np

SCENE_MAP={
    "1": "causal lunch drive",
    "2": "evening drive home",
    "3": "rush hour commute",
}

SUBCATEGORY_MAP = {
    "101": "route change",
    "102": "parking navigate",
    "103": "traffic update",
    "104": "lane change",
    "105": "friend ride",
    "201": "music",
    "202": "radio",
    "203": "drink order",
    "204": "package remind",
    "205": "IoT control",
}

def make_dummies():
    dummy_conditions = {
        "A": dict(
            recommendation_intensity=0.325,
            recommendation_coherence=0,
            car_density=0.3,
            time_pressure=0,
            mode="manual",
        ),

        # "B": dict(
        #     recommendation_intensity=0.325,
        #     recommendation_coherence=0,
        #     car_density=0.7,
        #     time_pressure=0,
        #     mode="manual",
        # ),

        "C": dict(
            recommendation_intensity=0.325,
            recommendation_coherence=0,
            car_density=0.7,
            time_pressure=1,
            mode="manual",
        ),

        # "D": dict(
        #     recommendation_intensity=0.325,
        #     recommendation_coherence=0,
        #     car_density=0.7,
        #     time_pressure=1,
        #     mode="auto",
        # ),

        # "E": dict(
        #     recommendation_intensity=0.243,
        #     recommendation_coherence=0,
        #     car_density=0.7,
        #     time_pressure=1,
        #     mode="manual",
        # ),

        "F": dict(
            recommendation_intensity=0.243,
            recommendation_coherence=1,
            car_density=0.7,
            time_pressure=1,
            mode="manual",
        ),

        # "G": dict(
        #     recommendation_intensity=0.243,
        #     recommendation_coherence=0,
        #     car_density=0.3,
        #     time_pressure=1,
        #     mode="manual",
        # ),

        "H": dict(
            recommendation_intensity=0.0,
            recommendation_coherence=0,
            car_density=0.3,
            time_pressure=1,
            mode="manual",
        ),
    }

    dummy_rows = {}

    for cond_name, overrides in dummy_conditions.items():

        row_dict = {

            # ---------------- meta ----------------
            "sub_id": 0,
            "scene_id": "scene_1",
            "mode": "manual",
            "trial_id": 0,
            "recommendation": "dummy recommendation",
            "recommendation_category": 1,
            "recommendation_subcategory": 101,

            # ---------------- prior ----------------
            "subject_prior": 0.5,
            "prior_mean_throttle_input": 0.0,
            "prior_var_throttle_input": 0.0,

            # ---------------- scenario ----------------
            "recommendation_intensity": 0.0,
            "recommendation_coherence": 0,
            "mean_throttle_input_zscore_pre2s": 0.0,
            "mean_throttle_pre2s": 0.0,
            "var_throttle_pre2s": 0.0,
            "car_density": 0.7,
            "time_pressure": 0,

            # ---------------- reaction ----------------
            "reaction": "ignore",
            "accept": False,
            "reaction_time_ms": np.nan,
            "mean_throttle_input_zscore_post5s": 0.0,
            "mean_throttle_post5s": 0.0,
            "var_throttle_post5s": 0.0,

            # ---------------- evaluation ----------------
            "appropriateness": 0.0,
            "disturbance": 0.0,
            "satisfaction": 0.0,

            # ---------------- bookkeeping ----------------
            "reaction__time_ms": 0.0,
            "trigger__time_ms": 0.0,
        }

        row_dict.update(overrides)

        dummy_rows[cond_name] = pd.Series(row_dict)

    return dummy_rows