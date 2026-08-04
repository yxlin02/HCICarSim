# HCI Car Simulator Study

This repository contains the driving-simulator project files, participant data, analysis notebook, reusable Python analysis modules, and generated outputs for a two-phase HCI study of in-car AI agent recommendations.

## Repository Overview

- `DataAnalysis.ipynb` is the main analysis notebook.
- `Data/` contains Phase I and Phase II participant data, rating questionnaires, prior-preference data, and recommendation-intensity feature files.
- `scripts/` contains reusable data-processing, modeling, plotting, machine-learning, and statistical-analysis code.
- `Output/` contains generated figures, tables, cross-validation results, LMM summaries, and sample-size sensitivity outputs.
- `Source/`, `Content/`, `Config/`, and `Plugins/` contain Unreal Engine simulator project assets and source files.

## Analysis Environment

Create the conda environment:

```bash
conda env create -f environment.yaml
conda activate hci-car-sim-analysis
```

Optional, but useful if running scripts from outside the repository root:

```bash
pip install -e .
```

The analysis code expects commands to be run from the repository root:

```bash
cd /path/to/HCICarSim
```

## Study Structure

Phase I used 24 participants. The default participant list is subjects `1-26` excluding `12` and `25`.

Phase II reused 12 participants from Phase I:

```text
3, 4, 5, 7, 8, 9, 11, 16, 19, 20, 22, 24
```

The Phase II LMM analysis intentionally combines each returning participant's Phase I `scene_3` baseline with their Phase II personalized and personified intervention drives. This supports within-participant comparisons while reducing between-sample heterogeneity.

## Main Analysis Workflow

The main notebook builds trial-level and reaction-level datasets, then runs:

- linear mixed-effects models for rating outcomes;
- subcategory and category-level statistical summaries;
- machine-learning analyses with subject-level validation;
- one-dimensional and two-dimensional dynamical model evaluations;
- figure and table generation.

The primary rating metrics are:

- `appropriateness`
- `disturbance`
- `satisfaction`

The machine-learning and dynamical-model analyses use leave-one-subject-out or subject-level validation where appropriate. The sample-size sensitivity analysis is therefore limited to the LMM analyses and does not duplicate the machine-learning validation pipeline.

## LMM Sample-Size Sensitivity Analysis

Run the standalone sensitivity script:

```bash
python scripts/lmm_sample_size_sensitivity.py
```

By default, the script uses the actual study design:

- Phase I: 24 participants, 720 trial-level rating observations.
- Phase II: 12 returning participants, 360 merged baseline/intervention rating observations.

The script mirrors the LMM specifications used in the notebook:

```text
Phase I:
metric ~ C(scene_id, Sum) + C(mode, Sum)

Phase II:
metric ~ C(version, Treatment(reference='default')) * C(mode, Sum)
```

Both models use participant-level random intercepts via `sub_id`.

The sensitivity analysis uses Wald/noncentral chi-square approximations and reports:

- observed Wald-style effect size, `f_like = sqrt(chi2 / n_obs)`;
- projected power at the observed sample size;
- participant counts needed for 80% and 90% power under the observed effect size;
- minimum detectable `f_like` effect sizes across candidate sample sizes.

Generated files are written to:

```text
Output/sample_size_sensitivity/
```

Key files:

- `lmm_sample_size_design.csv`
- `lmm_sample_size_sensitivity_effects.csv`
- `lmm_sample_size_sensitivity_grid.csv`
- `lmm_sample_size_sensitivity_summary.md`

To run only omnibus LMM effects:

```bash
python scripts/lmm_sample_size_sensitivity.py --effect-kind omnibus
```

To test alternative participant lists:

```bash
python scripts/lmm_sample_size_sensitivity.py \
  --phase-i-subjects 1,2,3,4,5,6,7,8,9,10,11,13,14,15,16,17,18,19,20,21,22,23,24,26 \
  --phase-ii-subjects 3,4,5,7,8,9,11,16,19,20,22,24
```

## Notes on Outputs

Many files in `Output/` are generated analysis artifacts. If rerunning the notebook or scripts, expect these files to change.

The currently generated sample-size sensitivity summary supports the following design description:

- Phase I provides sensitivity for medium-to-large LMM effects in the primary rating analyses.
- Phase II is a focused within-participant follow-up with sensitivity for medium-to-large version and mode effects.
- Smaller Phase II interaction effects should be interpreted cautiously.

## Data Use

The `Data/` directory contains participant-level research data. Treat these files as study data and do not redistribute them outside the intended research context without appropriate approval.
