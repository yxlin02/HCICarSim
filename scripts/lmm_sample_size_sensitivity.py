#!/usr/bin/env python3
"""
LMM-only sample-size sensitivity analysis for the two-stage HCI car study.

This script mirrors the LMM parts of DataAnalysis.ipynb:

- Phase I uses 24 participants by default:
  metric ~ C(scene_id, Sum) + C(mode, Sum), grouped by sub_id,
  fitted separately within each recommendation subcategory.

- Phase II uses 12 participants who were reused from Phase I by default:
  baseline = Phase I scene_3 for those participants,
  intervention = Phase II personalized/personified drives,
  metric ~ C(version, Treatment(reference='default')) * C(mode, Sum),
  grouped by sub_id, fitted separately within each recommendation subcategory.

The sensitivity layer uses a Wald/noncentral-chi-square approximation:

    f_like = sqrt(chi2 / n_obs)
    lambda(N) = f_like^2 * projected_n_obs(N)

It reports projected power for observed LMM effects and the minimum detectable
f_like effect size (MDE) at the requested power levels.
"""

from __future__ import annotations

import argparse
import math
import sys
import warnings
from collections import defaultdict
from pathlib import Path
from typing import Iterable

import numpy as np
import pandas as pd
import statsmodels.formula.api as smf
from scipy.stats import chi2, ncx2

PROJECT_ROOT_FOR_IMPORTS = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT_FOR_IMPORTS) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT_FOR_IMPORTS))

from scripts import databuilder, intensity_calculation, readsubcategoryrating


DEFAULT_PHASE_I_SUBJECTS = [s for s in range(1, 27) if s not in (12, 25)]
DEFAULT_PHASE_II_SUBJECTS = [3, 4, 5, 7, 8, 9, 11, 16, 19, 20, 22, 24]
DEFAULT_METRICS = ["appropriateness", "disturbance", "satisfaction"]


def parse_int_list(value: str) -> list[int]:
    return [int(x.strip()) for x in value.split(",") if x.strip()]


def read_car_marker_pair(sub_dir: Path) -> tuple[pd.DataFrame, pd.DataFrame]:
    marker_df = pd.read_csv(sub_dir / "Marker.csv")
    car_df = pd.read_csv(sub_dir / "PawnCar.csv", index_col=0)

    marker_df["_time_ms"] = pd.to_numeric(marker_df["unixtimestamp_ms"], errors="coerce")
    car_df["_time_ms"] = pd.to_numeric(car_df.index, errors="coerce")
    car_df = car_df.reset_index(drop=True)

    marker_df.columns = marker_df.columns.str.strip().str.lower()
    car_df.columns = car_df.columns.str.strip().str.lower()

    return marker_df, car_df


def intensity_for_dir(sub_dir: Path, fallback_path: Path) -> dict:
    stimuli_file_path = sub_dir / "recommendation_intensity_features.csv"
    if stimuli_file_path.exists():
        return intensity_calculation.intensity_calculation(stimuli_file_path)
    return intensity_calculation.intensity_calculation(fallback_path)


def load_subject_prior(data_root: Path) -> pd.DataFrame:
    df_subject_prior = pd.read_csv(data_root / "sub_prior.csv")
    return readsubcategoryrating.format_prior(df_subject_prior)


def build_phase_i_reaction_df(
    project_root: Path,
    subjects: Iterable[int],
    pre_window_ms: int = 2000,
    post_window_ms: int = 5000,
) -> pd.DataFrame:
    data_root = project_root / "Data"
    data_dir = data_root / "phaseI"

    df_phase = pd.read_csv(data_dir / "PhaseI_Block.csv")
    df_pattern = pd.read_csv(data_dir / "RecommendationPatterns.csv")
    df_subcategory_rating = readsubcategoryrating.format(
        pd.read_csv(data_dir / "Rating_subcategory.csv")
    )
    df_subject_prior = load_subject_prior(data_root)
    stimuli_intensity_dict = intensity_calculation.intensity_calculation(
        data_dir / "recommendation_intensity_features.csv"
    )

    data_dict = defaultdict(dict)

    for sub in subjects:
        sub_key = f"sub_{sub}"
        sub_dirs = sorted(data_dir.glob(f"sub_{sub}_*"))
        if len(sub_dirs) != 3:
            raise ValueError(f"Phase I subject {sub} has {len(sub_dirs)} folders, expected 3.")

        for j, sub_dir in enumerate(sub_dirs):
            block = j + 4 if sub % 2 == 0 else j + 1
            phase_rows = df_phase[(df_phase["Sub"] == sub) & (df_phase["Block"] == block)]
            if phase_rows.empty:
                raise ValueError(f"Missing PhaseI_Block row for subject={sub}, block={block}.")

            row = phase_rows.iloc[0]
            scene_key = f"scene_{row['SceneID']}"

            sub_rating = df_subcategory_rating[
                (df_subcategory_rating["sub_id"] == sub)
                & (df_subcategory_rating["block_id"] == block)
            ]
            if sub_rating.empty:
                raise ValueError(f"Missing Phase I rating for subject={sub}, block={block}.")

            marker_df, car_df = read_car_marker_pair(sub_dir)

            stimuli_file_path = sub_dir / "recommendation_intensity_features.csv"
            if stimuli_file_path.exists():
                sub_stimuli_intensity_dict = intensity_calculation.intensity_calculation(
                    stimuli_file_path
                )
            else:
                sub_stimuli_intensity_dict = stimuli_intensity_dict

            data_dict[sub_key][scene_key] = {
                "marker": marker_df,
                "car": car_df,
                "subcategory_rating": sub_rating,
                "stimuli_intensity": sub_stimuli_intensity_dict,
                "pattern": df_pattern[df_pattern["PatternID"] == row["PatternID"]],
            }

    return databuilder.build_per_reaction_df(
        data_dict,
        pre_window_ms=pre_window_ms,
        post_window_ms=post_window_ms,
        df_subject_prior=df_subject_prior,
    )


def build_phase_ii_merged_reaction_df(
    project_root: Path,
    phase_i_reaction_df: pd.DataFrame,
    subjects: Iterable[int],
    pre_window_ms: int = 2000,
    post_window_ms: int = 5000,
) -> pd.DataFrame:
    data_root = project_root / "Data"
    data_dir_i = data_root / "phaseI"
    data_dir_ii = data_root / "phaseII"

    df_subcategory_rating_ii = readsubcategoryrating.format(
        pd.read_csv(data_dir_ii / "Rating_subcategory.csv")
    )
    df_subject_prior = load_subject_prior(data_root)

    fallback_intensity_path = data_dir_i / "recommendation_intensity_features.csv"
    data_dict_ii = defaultdict(dict)

    for sub in subjects:
        sub_key = f"sub_{sub}"
        sub_dirs = sorted(data_dir_ii.glob(f"sub_{sub}_*"))
        if len(sub_dirs) != 2:
            raise ValueError(f"Phase II subject {sub} has {len(sub_dirs)} folders, expected 2.")

        for j, sub_dir in enumerate(sub_dirs):
            scene_key = (
                "scene_4"
                if (sub % 2 == 1 and j % 2 == 0) or (sub % 2 == 0 and j % 2 == 1)
                else "scene_5"
            )
            version = "personalized" if scene_key == "scene_4" else "personified"

            sub_rating = df_subcategory_rating_ii[
                (df_subcategory_rating_ii["sub_id"] == sub)
                & (df_subcategory_rating_ii["version"].str.lower() == version)
            ]
            if sub_rating.empty:
                raise ValueError(f"Missing Phase II rating for subject={sub}, version={version}.")

            marker_df, car_df = read_car_marker_pair(sub_dir)

            data_dict_ii[sub_key][scene_key] = {
                "marker": marker_df,
                "car": car_df,
                "subcategory_rating": sub_rating,
                "stimuli_intensity": intensity_for_dir(sub_dir, fallback_intensity_path),
            }

    df_reaction_ii = databuilder.build_per_reaction_df(
        data_dict_ii,
        pre_window_ms=pre_window_ms,
        post_window_ms=post_window_ms,
        df_subject_prior=df_subject_prior,
    )

    subjects = list(subjects)
    df_reaction_baseline = phase_i_reaction_df[
        (phase_i_reaction_df["sub_id"].isin(subjects))
        & (phase_i_reaction_df["scene_id"] == "scene_3")
    ].copy()

    return pd.concat([df_reaction_baseline, df_reaction_ii], ignore_index=True)


def as_categorical(df: pd.DataFrame, cols: Iterable[str]) -> pd.DataFrame:
    out = df.copy()
    for col in cols:
        if col in out.columns and not isinstance(out[col].dtype, pd.CategoricalDtype):
            out[col] = out[col].astype("category")
    return out


def wald_pvalue(test_obj) -> float:
    return float(np.asarray(test_obj.pvalue).reshape(-1)[0])


def wald_stat(test_obj) -> float:
    return float(np.asarray(test_obj.statistic).reshape(-1)[0])


def get_terms(
    model,
    include: Iterable[str],
    exclude: Iterable[str] | None = None,
    interaction: bool = False,
) -> list[str]:
    if exclude is None:
        exclude = []

    terms = []
    for term in model.params.index:
        if term in ("Intercept", "Group Var"):
            continue

        ok = all(k in term for k in include)
        ok = ok and all(k not in term for k in exclude)
        ok = ok and ((":" in term) if interaction else (":" not in term))

        if ok:
            terms.append(term)

    return terms


def joint_wald_from_terms(model, terms: list[str]) -> dict:
    if not terms:
        return {"chi2": np.nan, "df": np.nan, "p": np.nan, "terms": ""}

    expr = ", ".join([f"{term} = 0" for term in terms])
    try:
        test = model.wald_test(expr, scalar=True)
        return {
            "chi2": wald_stat(test),
            "df": len(terms),
            "p": wald_pvalue(test),
            "terms": "; ".join(terms),
        }
    except Exception as exc:
        warnings.warn(f"Wald test failed for {expr}: {exc}")
        return {"chi2": np.nan, "df": len(terms), "p": np.nan, "terms": "; ".join(terms)}


def fit_mixedlm(formula: str, df: pd.DataFrame):
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        model = smf.mixedlm(formula, df, groups=df["sub_id"])
        for method in ("lbfgs", "powell", "cg", "nm"):
            try:
                result = model.fit(reml=False, method=method, disp=False)
                if getattr(result, "converged", True):
                    return result
            except Exception:
                continue

        return model.fit(reml=False, disp=False)


def add_term_rows(
    rows: list[dict],
    phase: str,
    subcategory,
    metric: str,
    model,
    df_sub: pd.DataFrame,
    include_terms: bool,
) -> None:
    if not include_terms:
        return

    for term in model.params.index:
        if term in ("Intercept", "Group Var"):
            continue

        stat = float(model.tvalues.get(term, np.nan))
        chi_square = stat * stat if not np.isnan(stat) else np.nan

        rows.append(
            {
                "phase": phase,
                "effect_kind": "term",
                "subcategory": subcategory,
                "metric": metric,
                "effect": term,
                "terms": term,
                "df": 1,
                "chi2": chi_square,
                "p": float(model.pvalues.get(term, np.nan)),
                "n_obs": int(model.nobs),
                "n_subjects": int(df_sub["sub_id"].nunique()),
                "converged": getattr(model, "converged", np.nan),
            }
        )


def fit_phase_i_lmms(
    df: pd.DataFrame,
    metrics: Iterable[str],
    include_terms: bool,
) -> pd.DataFrame:
    df = as_categorical(
        df,
        ["recommendation_subcategory", "recommendation_category", "scene_id", "mode", "sub_id"],
    )
    rows = []

    for subcat in list(df["recommendation_subcategory"].cat.categories):
        df_sub = df[df["recommendation_subcategory"] == subcat].copy()
        if df_sub.empty:
            continue

        for metric in metrics:
            formula = f"{metric} ~ C(scene_id, Sum) + C(mode, Sum)"
            try:
                model = fit_mixedlm(formula, df_sub)
            except Exception as exc:
                warnings.warn(f"Phase I LMM failed for subcategory={subcat}, metric={metric}: {exc}")
                continue

            effect_specs = {
                "scene": get_terms(
                    model,
                    include=["C(scene_id, Sum)"],
                    exclude=["C(mode, Sum)"],
                    interaction=False,
                ),
                "mode": get_terms(
                    model,
                    include=["C(mode, Sum)"],
                    exclude=["C(scene_id, Sum)"],
                    interaction=False,
                ),
            }

            for effect, terms in effect_specs.items():
                test = joint_wald_from_terms(model, terms)
                rows.append(
                    {
                        "phase": "phase_I",
                        "effect_kind": "omnibus",
                        "subcategory": subcat,
                        "metric": metric,
                        "effect": effect,
                        "terms": test["terms"],
                        "df": test["df"],
                        "chi2": test["chi2"],
                        "p": test["p"],
                        "n_obs": int(model.nobs),
                        "n_subjects": int(df_sub["sub_id"].nunique()),
                        "converged": getattr(model, "converged", np.nan),
                    }
                )

            add_term_rows(rows, "phase_I", subcat, metric, model, df_sub, include_terms)

    return pd.DataFrame(rows)


def fit_phase_ii_lmms(
    df: pd.DataFrame,
    metrics: Iterable[str],
    include_terms: bool,
) -> pd.DataFrame:
    df = as_categorical(
        df,
        ["recommendation_subcategory", "recommendation_category", "version", "mode", "sub_id"],
    )
    rows = []

    for subcat in list(df["recommendation_subcategory"].cat.categories):
        df_sub = df[df["recommendation_subcategory"] == subcat].copy()
        if df_sub.empty:
            continue

        for metric in metrics:
            formula = (
                f"{metric} ~ "
                "C(version, Treatment(reference='default')) * "
                "C(mode, Sum)"
            )
            try:
                model = fit_mixedlm(formula, df_sub)
            except Exception as exc:
                warnings.warn(f"Phase II LMM failed for subcategory={subcat}, metric={metric}: {exc}")
                continue

            effect_specs = {
                "version": get_terms(
                    model,
                    include=["C(version"],
                    exclude=["C(mode"],
                    interaction=False,
                ),
                "mode": get_terms(
                    model,
                    include=["C(mode"],
                    exclude=["C(version"],
                    interaction=False,
                ),
                "version_x_mode": get_terms(
                    model,
                    include=["C(version", "C(mode"],
                    interaction=True,
                ),
            }

            for effect, terms in effect_specs.items():
                test = joint_wald_from_terms(model, terms)
                rows.append(
                    {
                        "phase": "phase_II",
                        "effect_kind": "omnibus",
                        "subcategory": subcat,
                        "metric": metric,
                        "effect": effect,
                        "terms": test["terms"],
                        "df": test["df"],
                        "chi2": test["chi2"],
                        "p": test["p"],
                        "n_obs": int(model.nobs),
                        "n_subjects": int(df_sub["sub_id"].nunique()),
                        "converged": getattr(model, "converged", np.nan),
                    }
                )

            add_term_rows(rows, "phase_II", subcat, metric, model, df_sub, include_terms)

    return pd.DataFrame(rows)


def power_from_ncp(df_effect: int, ncp: float, alpha: float) -> float:
    if pd.isna(df_effect) or df_effect <= 0 or pd.isna(ncp):
        return np.nan
    crit = chi2.ppf(1.0 - alpha, df=df_effect)
    return float(ncx2.sf(crit, df=df_effect, nc=ncp))


def ncp_for_power(df_effect: int, alpha: float, target_power: float) -> float:
    if target_power <= alpha:
        return 0.0

    lo = 0.0
    hi = 1.0
    while power_from_ncp(df_effect, hi, alpha) < target_power:
        hi *= 2.0
        if hi > 1e6:
            raise RuntimeError("Could not bracket noncentrality parameter.")

    for _ in range(80):
        mid = (lo + hi) / 2.0
        if power_from_ncp(df_effect, mid, alpha) < target_power:
            lo = mid
        else:
            hi = mid

    return hi


def enrich_effect_rows(
    effects_df: pd.DataFrame,
    alpha: float,
    target_powers: Iterable[float],
    max_n_search: int,
) -> pd.DataFrame:
    out = effects_df.copy()
    out["obs_per_subject"] = out["n_obs"] / out["n_subjects"]
    out["f_like_observed"] = np.sqrt(out["chi2"] / out["n_obs"])
    out["power_at_observed_n"] = np.nan

    for target_power in target_powers:
        out[f"n_for_power_{int(round(target_power * 100))}"] = np.nan

    for idx, row in out.iterrows():
        df_effect = int(row["df"]) if not pd.isna(row["df"]) else None
        f_like = row["f_like_observed"]
        obs_per_subject = row["obs_per_subject"]

        if df_effect is None or df_effect <= 0 or pd.isna(f_like) or pd.isna(obs_per_subject):
            continue

        current_ncp = (f_like**2) * row["n_obs"]
        out.loc[idx, "power_at_observed_n"] = power_from_ncp(df_effect, current_ncp, alpha)

        for target_power in target_powers:
            col = f"n_for_power_{int(round(target_power * 100))}"
            for n_subjects in range(2, max_n_search + 1):
                projected_n_obs = obs_per_subject * n_subjects
                projected_ncp = (f_like**2) * projected_n_obs
                if power_from_ncp(df_effect, projected_ncp, alpha) >= target_power:
                    out.loc[idx, col] = n_subjects
                    break

    return out


def build_grid(
    effects_df: pd.DataFrame,
    phase_i_grid: tuple[int, int],
    phase_ii_grid: tuple[int, int],
    grid_step: int,
    alpha: float,
    target_powers: Iterable[float],
) -> pd.DataFrame:
    lambda_cache: dict[tuple[int, float], float] = {}
    rows = []

    for _, effect_row in effects_df.iterrows():
        df_effect = effect_row["df"]
        f_like = effect_row["f_like_observed"]
        obs_per_subject = effect_row["obs_per_subject"]

        if pd.isna(df_effect) or df_effect <= 0 or pd.isna(obs_per_subject):
            continue

        df_effect = int(df_effect)
        if effect_row["phase"] == "phase_I":
            start, stop = phase_i_grid
        else:
            start, stop = phase_ii_grid

        for n_subjects in range(start, stop + 1, grid_step):
            projected_n_obs = obs_per_subject * n_subjects
            projected_ncp = (f_like**2) * projected_n_obs if not pd.isna(f_like) else np.nan
            row = {
                "phase": effect_row["phase"],
                "effect_kind": effect_row["effect_kind"],
                "subcategory": effect_row["subcategory"],
                "metric": effect_row["metric"],
                "effect": effect_row["effect"],
                "df": df_effect,
                "n_subjects": n_subjects,
                "projected_n_obs": projected_n_obs,
                "power_for_observed_effect": power_from_ncp(df_effect, projected_ncp, alpha),
            }

            for target_power in target_powers:
                cache_key = (df_effect, float(target_power))
                if cache_key not in lambda_cache:
                    lambda_cache[cache_key] = ncp_for_power(df_effect, alpha, target_power)
                lambda_req = lambda_cache[cache_key]
                mde = math.sqrt(lambda_req / projected_n_obs) if projected_n_obs > 0 else np.nan
                row[f"mde_f_like_for_power_{int(round(target_power * 100))}"] = mde

            rows.append(row)

    return pd.DataFrame(rows)


def summarize_design(
    phase_i_df: pd.DataFrame,
    phase_ii_df: pd.DataFrame,
    phase_i_subjects: list[int],
    phase_ii_subjects: list[int],
) -> pd.DataFrame:
    return pd.DataFrame(
        [
            {
                "phase": "phase_I",
                "planned_or_observed_subjects": len(phase_i_subjects),
                "subject_ids": ",".join(map(str, phase_i_subjects)),
                "n_rows": len(phase_i_df),
                "n_unique_subjects": phase_i_df["sub_id"].nunique(),
                "notes": "Phase I LMM dataset.",
            },
            {
                "phase": "phase_II",
                "planned_or_observed_subjects": len(phase_ii_subjects),
                "subject_ids": ",".join(map(str, phase_ii_subjects)),
                "n_rows": len(phase_ii_df),
                "n_unique_subjects": phase_ii_df["sub_id"].nunique(),
                "notes": "Phase II merged LMM dataset: Phase I scene_3 baseline plus Phase II intervention rows.",
            },
        ]
    )


def write_markdown_summary(
    path: Path,
    design_df: pd.DataFrame,
    effects_df: pd.DataFrame,
    target_powers: Iterable[float],
) -> None:
    lines = [
        "# LMM Sample-Size Sensitivity Summary",
        "",
        "This file is generated by `scripts/lmm_sample_size_sensitivity.py`.",
        "",
        "## Design",
        "",
        design_df.to_markdown(index=False),
        "",
        "## Current-N Sensitivity",
        "",
    ]

    omnibus = effects_df[effects_df["effect_kind"] == "omnibus"].copy()
    for phase in ["phase_I", "phase_II"]:
        phase_df = omnibus[omnibus["phase"] == phase]
        if phase_df.empty:
            continue

        lines.append(f"### {phase}")
        lines.append("")
        rows = []
        for effect in sorted(phase_df["effect"].dropna().unique()):
            effect_df = phase_df[phase_df["effect"] == effect]
            row = {
                "effect": effect,
                "tests": len(effect_df),
                "median_observed_f_like": effect_df["f_like_observed"].median(),
                "median_power_at_current_n": effect_df["power_at_observed_n"].median(),
            }
            for target_power in target_powers:
                col = f"n_for_power_{int(round(target_power * 100))}"
                reached = effect_df[col].notna()
                row[f"reached_{int(round(target_power * 100))}_within_search"] = int(reached.sum())
                row[f"median_{col}_among_reached"] = effect_df.loc[reached, col].median()
            rows.append(row)

        summary_table = pd.DataFrame(rows)
        lines.append(summary_table.to_markdown(index=False, floatfmt=".3f"))
        lines.append("")

    lines.extend(
        [
            "## Interpretation Notes",
            "",
            "- `f_like_observed = sqrt(Wald chi-square / n_obs)` follows the effect-size convention already used in the notebook summaries.",
            "- `power_for_observed_effect` is a sensitivity/projection quantity, not a new confirmatory test.",
            "- `mde_f_like` is the minimum detectable Wald-style effect size under the same repeated-measures design density.",
            "- Phase II explicitly models the reused-subject design by combining each reused participant's Phase I `scene_3` baseline with their Phase II rows.",
            "",
        ]
    )

    path.write_text("\n".join(lines), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run LMM-only participant-number sensitivity analysis."
    )
    parser.add_argument("--project-root", type=Path, default=Path.cwd())
    parser.add_argument("--out-dir", type=Path, default=Path("Output/sample_size_sensitivity"))
    parser.add_argument("--phase-i-subjects", type=parse_int_list, default=DEFAULT_PHASE_I_SUBJECTS)
    parser.add_argument("--phase-ii-subjects", type=parse_int_list, default=DEFAULT_PHASE_II_SUBJECTS)
    parser.add_argument("--metrics", type=lambda s: [x.strip() for x in s.split(",") if x.strip()], default=DEFAULT_METRICS)
    parser.add_argument("--phase-i-grid", nargs=2, type=int, default=(8, 40), metavar=("MIN", "MAX"))
    parser.add_argument("--phase-ii-grid", nargs=2, type=int, default=(6, 30), metavar=("MIN", "MAX"))
    parser.add_argument("--grid-step", type=int, default=1)
    parser.add_argument("--alpha", type=float, default=0.05)
    parser.add_argument("--target-powers", type=float, nargs="+", default=[0.80, 0.90])
    parser.add_argument("--max-n-search", type=int, default=80)
    parser.add_argument(
        "--effect-kind",
        choices=["omnibus", "term", "both"],
        default="both",
        help="Which LMM effects to keep in the sensitivity output.",
    )
    parser.add_argument("--pre-window-ms", type=int, default=2000)
    parser.add_argument("--post-window-ms", type=int, default=5000)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    project_root = args.project_root.resolve()
    out_dir = (project_root / args.out_dir).resolve() if not args.out_dir.is_absolute() else args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    phase_i_subjects = list(args.phase_i_subjects)
    phase_ii_subjects = list(args.phase_ii_subjects)
    include_terms = args.effect_kind in ("term", "both")

    print("Building Phase I reaction dataframe...")
    phase_i_df = build_phase_i_reaction_df(
        project_root,
        phase_i_subjects,
        pre_window_ms=args.pre_window_ms,
        post_window_ms=args.post_window_ms,
    )

    print("Building Phase II merged reaction dataframe...")
    phase_ii_df = build_phase_ii_merged_reaction_df(
        project_root,
        phase_i_df,
        phase_ii_subjects,
        pre_window_ms=args.pre_window_ms,
        post_window_ms=args.post_window_ms,
    )

    design_df = summarize_design(phase_i_df, phase_ii_df, phase_i_subjects, phase_ii_subjects)

    print("Fitting Phase I LMMs...")
    phase_i_effects = fit_phase_i_lmms(phase_i_df, args.metrics, include_terms=include_terms)

    print("Fitting Phase II LMMs...")
    phase_ii_effects = fit_phase_ii_lmms(phase_ii_df, args.metrics, include_terms=include_terms)

    effects_df = pd.concat([phase_i_effects, phase_ii_effects], ignore_index=True)
    if args.effect_kind != "both":
        effects_df = effects_df[effects_df["effect_kind"] == args.effect_kind].copy()

    effects_df = enrich_effect_rows(
        effects_df,
        alpha=args.alpha,
        target_powers=args.target_powers,
        max_n_search=args.max_n_search,
    )

    grid_df = build_grid(
        effects_df,
        phase_i_grid=tuple(args.phase_i_grid),
        phase_ii_grid=tuple(args.phase_ii_grid),
        grid_step=args.grid_step,
        alpha=args.alpha,
        target_powers=args.target_powers,
    )

    design_path = out_dir / "lmm_sample_size_design.csv"
    effects_path = out_dir / "lmm_sample_size_sensitivity_effects.csv"
    grid_path = out_dir / "lmm_sample_size_sensitivity_grid.csv"
    summary_path = out_dir / "lmm_sample_size_sensitivity_summary.md"

    design_df.to_csv(design_path, index=False)
    effects_df.to_csv(effects_path, index=False)
    grid_df.to_csv(grid_path, index=False)
    write_markdown_summary(summary_path, design_df, effects_df, args.target_powers)

    print(f"Wrote {design_path}")
    print(f"Wrote {effects_path}")
    print(f"Wrote {grid_path}")
    print(f"Wrote {summary_path}")


if __name__ == "__main__":
    main()
