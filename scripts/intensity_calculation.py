
import pandas as pd

def intensity_calculation(
    csv_path,
    modality_weights=None,
    feature_weights=None,
    categorical_maps=None,
):
    """
    Read recommendation feature csv and return {id: intensity_score}.

    Parameters
    ----------
    csv_path : str
        Path to the csv file.
    modality_weights : dict or None
        Example:
        {
            "content": 1.0,
            "sound": 1.0,
            "visual": 1.0,
            "haptic": 1.0,
        }
    feature_weights : dict or None
        Example:
        {
            "content_urgency": 0.6,
            "content_info_load": 0.4,
            "sound_duration": 0.7,
            "sound_volume": 0.15,
            "sound_pitch": 0.15,
            "visual_complexity": 0.5,
            "visual_size": 0.5,
            "haptic_intensity": 0.7,
            "haptic_duration": 0.3,
        }
    categorical_maps : dict or None
        Example:
        {
            "sound_volume": {"low": 1, "medium": 2, "high": 3},
            "sound_pitch": {"low": 1, "medium": 2, "high": 3},
            "visual_complexity": {"low": 1, "medium": 2, "high": 3},
            "visual_size": {"small": 1, "medium": 2, "large": 3},
            "haptic_intensity": {"low": 1, "medium": 2, "high": 3},
        }
    """
    df = pd.read_csv(csv_path)

    if modality_weights is None:
        modality_weights = {
            "content": 2.0,
            "sound": 1.0,
            "visual": 1.0,
            "haptic": 1.0,
        }

    if feature_weights is None:
        feature_weights = {
            "content_urgency": 0.6,
            "content_info_load": 0.4,
            "sound_duration": 0.7,
            "sound_volume": 0.15,
            "sound_pitch": 0.15,
            "visual_complexity": 0.5,
            "visual_size": 0.5,
            "haptic_intensity": 0.7,
            "haptic_duration": 0.3,
        }

    if categorical_maps is None:
        categorical_maps = {
            "sound_volume": {"low": 1, "medium": 2, "high": 3},
            "sound_pitch": {"low": 1, "medium": 2, "high": 3},
            "visual_complexity": {"low": 1, "medium": 2, "high": 3},
            "visual_size": {"small": 1, "medium": 2, "large": 3},
            "haptic_intensity": {"low": 1, "medium": 2, "high": 3},
        }

    df = df.copy()

    for col, mapper in categorical_maps.items():
        if col in df.columns:
            df[col] = df[col].map(mapper).astype(float)

    numeric_cols = [
        "content_urgency",
        "content_info_load",
        "sound_duration",
        "sound_volume",
        "sound_pitch",
        "visual_complexity",
        "visual_size",
        "haptic_intensity",
        "haptic_duration",
    ]

    # Min-max normalize each feature to [0, 1]
    for col in numeric_cols:
        x = df[col].astype(float)
        xmin, xmax = x.min(), x.max()
        if xmax > xmin:
            df[col + "_norm"] = (x - xmin) / (xmax - xmin)
        else:
            df[col + "_norm"] = 0.0

    def weighted_sum(cols):
        total_w = sum(feature_weights[c] for c in cols)
        if total_w == 0:
            return 0.0
        return sum(feature_weights[c] * row[c + "_norm"] for c in cols) / total_w

    out = {}
    for _, row in df.iterrows():
        content_score = (
            feature_weights["content_urgency"] * row["content_urgency_norm"]
            + feature_weights["content_info_load"] * row["content_info_load_norm"]
        ) / (feature_weights["content_urgency"] + feature_weights["content_info_load"])

        sound_score = (
            feature_weights["sound_duration"] * row["sound_duration_norm"]
            + feature_weights["sound_volume"] * row["sound_volume_norm"]
            + feature_weights["sound_pitch"] * row["sound_pitch_norm"]
        ) / (feature_weights["sound_duration"] + feature_weights["sound_volume"] + feature_weights["sound_pitch"])

        visual_score = (
            feature_weights["visual_complexity"] * row["visual_complexity_norm"]
            + feature_weights["visual_size"] * row["visual_size_norm"]
        ) / (feature_weights["visual_complexity"] + feature_weights["visual_size"])

        haptic_score = (
            feature_weights["haptic_intensity"] * row["haptic_intensity_norm"]
            + feature_weights["haptic_duration"] * row["haptic_duration_norm"]
        ) / (feature_weights["haptic_intensity"] + feature_weights["haptic_duration"])

        total_modality_weight = sum(modality_weights.values())
        intensity = (
            modality_weights["content"] * content_score
            + modality_weights["sound"] * sound_score
            + modality_weights["visual"] * visual_score
            + modality_weights["haptic"] * haptic_score
        ) / total_modality_weight

        out[int(row["id"])] = float(intensity)

    return out
