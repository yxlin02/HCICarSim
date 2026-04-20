import numpy as np
import matplotlib.pyplot as plt
import pandas as pd


# =========================
# 1. Basic utility functions
# =========================

def safe_logit(p, eps=1e-6):
    p = np.clip(p, eps, 1 - eps)
    return np.log(p / (1 - p))


def sigmoid(z):
    return 1.0 / (1.0 + np.exp(-z))


# =========================
# 2. Input drive u
# =========================
# Clean version:
# u = recommendation evidence - immediate behavioral resistance
#
# Here I keep throttle_pressure in u as a direct "local reject pressure".
# Context variables like car_density / time_pressure / mode are handled
# separately by lam_eff / sigma_eff / gain_eff.

def compute_drive_u(
    coherence,
    intensity,
    throttle_pressure=0.0,
    car_density=None,
    time_pressure=None,
    mode=None,
    beta_c=1.2,
    beta_i=0.8,
    beta_ci=0.5,
    gamma_throttle=1.0,
    gamma_density=0.0,
    gamma_time=0.0,
    gamma_mode=0.0,
):
    """
    Recommendation-side drive:
        u = evidence - direct local resistance

    By default, density/time/mode are NOT included here to avoid
    double-counting context, since they are already mapped into
    lam_eff / sigma_eff / gain_eff.
    """
    u = (
        beta_c * coherence
        + beta_i * intensity
        + beta_ci * coherence * intensity
    )

    # direct immediate resistance
    u -= gamma_throttle * throttle_pressure

    # kept for backward compatibility, but default to 0.0
    if car_density is not None:
        u -= gamma_density * car_density

    if time_pressure and gamma_time != 0.0:
        u -= gamma_time

    if mode is not None and gamma_mode != 0.0:
        if mode == "manual":
            u -= gamma_mode
        else:
            u += gamma_mode

    return u


# =========================
# 3. Initial state from prior
# =========================

def compute_x0_from_prior(prior_accept_prob, kappa=1.0, noise_std=0.02, eps=1e-6):
    p = prior_accept_prob + np.random.normal(0, noise_std)
    p = np.clip(p, eps, 1 - eps)
    return kappa * safe_logit(p)


# =========================
# 4. Core nonlinear term
# =========================

def nonlinear_phi(x, model="nonlinear_tanh", gain=1.5, theta_dyn=0.0):
    """
    Centered nonlinear activation.
    Returns a bounded term in roughly [-1, 1].
    """
    z = gain * (x - theta_dyn)

    if model == "linear":
        return 0.0
    elif model == "nonlinear_tanh":
        return np.tanh(z)
    elif model == "nonlinear_sigmoid":
        return 2.0 * sigmoid(z) - 1.0
    else:
        raise ValueError(
            "model must be one of: 'linear', 'nonlinear_tanh', 'nonlinear_sigmoid'"
        )


def decision_drift(
    x,
    u,
    lam=1.0,
    a=1.2,
    gain=1.5,
    theta_dyn=0.0,
    x_reject_rest=0,
    stim_on=1.0,
    model="nonlinear_tanh",
):
    if model == "linear":
        return -lam * (x - x_reject_rest) + u

    phi = nonlinear_phi(x, model=model, gain=gain, theta_dyn=theta_dyn)
    return -lam * (x - x_reject_rest) + stim_on * a * phi + u


# =========================
# 5. Context mapping
# =========================

def apply_context_effects(
    lam=1.0,
    sigma=0.05,
    gain=1.5,
    car_density=0.0,
    time_pressure=False,
    mode="manual",
    density_lam_gain=0.5,
    density_sigma_gain=1.5,
    time_gain_multiplier=1.5,
    auto_lam_multiplier=1.2,
    auto_sigma_multiplier=0.7,
):
    """
    Map driving context into effective dynamical parameters.

    Interpretation:
    - lam_eff: stronger leak / resistance under higher task load
    - sigma_eff: more stochastic variability under denser traffic
    - gain_eff: sharper / more urgent dynamics under time pressure
    """
    lam_eff = lam * (1.0 + density_lam_gain * car_density)
    sigma_eff = sigma * (1.0 + density_sigma_gain * car_density)
    gain_eff = gain * (time_gain_multiplier if time_pressure else 1.0)

    if mode == "auto":
        lam_eff *= auto_lam_multiplier
        sigma_eff *= auto_sigma_multiplier

    return lam_eff, sigma_eff, gain_eff


# =========================
# 6. Root / nullcline helpers
# =========================

def find_nullclines_1d(
    u,
    stim_on = 1.0,
    lam=1.0,
    a=1.2,
    gain=1.5,
    theta_dyn=0.0,
    model="nonlinear_tanh",
    x_min=-6.0,
    x_max=6.0,
    n=2000,
):
    """
    Find equilibrium points x* such that dx/dt = 0.
    In 1D, these are the nullclines / fixed points.
    """
    x_grid = np.linspace(x_min, x_max, n)
    y_grid = decision_drift(
        x_grid,
        u=u,
        stim_on=stim_on,
        lam=lam,
        a=a,
        gain=gain,
        theta_dyn=theta_dyn,
        model=model,
    )

    roots = []

    exact_idx = np.where(np.isclose(y_grid, 0.0, atol=1e-10))[0]
    for idx in exact_idx:
        roots.append(x_grid[idx])

    sign_change_idx = np.where(np.diff(np.sign(y_grid)) != 0)[0]
    for i in sign_change_idx:
        x1, x2 = x_grid[i], x_grid[i + 1]
        y1, y2 = y_grid[i], y_grid[i + 1]

        if np.isclose(y2 - y1, 0.0):
            continue

        xr = x1 - y1 * (x2 - x1) / (y2 - y1)
        roots.append(xr)

    if not roots:
        return []

    roots = np.array(sorted(roots))

    dedup = [roots[0]]
    for r in roots[1:]:
        if abs(r - dedup[-1]) > 1e-3:
            dedup.append(r)

    return dedup


def classify_fixed_point_stability(
    x_star,
    u,
    stim_on=1.0,
    lam=1.0,
    a=1.2,
    gain=1.5,
    theta_dyn=0.0,
    model="nonlinear_tanh",
    eps=1e-4,
):
    """
    In 1D:
    stable if d/dx (dx/dt) < 0 at fixed point
    unstable if > 0
    """
    f_plus = decision_drift(
        x_star + eps, u=u, lam=lam, a=a, gain=gain, theta_dyn=theta_dyn, stim_on=stim_on, model=model
    )
    f_minus = decision_drift(
        x_star - eps, u=u, lam=lam, a=a, gain=gain, theta_dyn=theta_dyn, stim_on=stim_on, model=model
    )
    deriv = (f_plus - f_minus) / (2.0 * eps)

    if deriv < 0:
        return "stable", deriv
    elif deriv > 0:
        return "unstable", deriv
    else:
        return "neutral", deriv


# =========================
# 7. 1D simulation
# =========================

def simulate_decision_1d(
    x0,
    u,
    stim_on,
    lam=1.0,
    a=1.2,
    gain=1.5,
    theta_dyn=0.0,
    car_density=0.0,
    time_pressure=False,
    mode="manual",
    sigma=0.05,
    dt=0.01,
    T=3.0,
    theta_readout=0.0,
    model="nonlinear_tanh",
    random_state=None,
    density_lam_gain=0.5,
    density_sigma_gain=1.5,
    time_gain_multiplier=1.5,
    auto_lam_multiplier=1.2,
    auto_sigma_multiplier=0.7,
):
    """
    Simulate:
        dx = f(x) dt + sigma_eff * sqrt(dt) * N(0,1)

    model:
        - 'linear'
        - 'nonlinear_tanh'
        - 'nonlinear_sigmoid'
    """
    rng = np.random.default_rng(random_state)

    lam_eff, sigma_eff, gain_eff = apply_context_effects(
        lam=lam,
        sigma=sigma,
        gain=gain,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        density_lam_gain=density_lam_gain,
        density_sigma_gain=density_sigma_gain,
        time_gain_multiplier=time_gain_multiplier,
        auto_lam_multiplier=auto_lam_multiplier,
        auto_sigma_multiplier=auto_sigma_multiplier,
    )

    n_steps = int(T / dt) + 1
    t = np.linspace(0, T, n_steps)

    x = np.zeros(n_steps, dtype=float)
    x[0] = x0

    for k in range(n_steps - 1):
        drift = decision_drift(
            x[k],
            u=u,
            stim_on=stim_on,
            lam=lam_eff,
            a=a,
            gain=gain_eff,
            theta_dyn=theta_dyn,
            model=model,
        )
        noise = sigma_eff * np.sqrt(dt) * rng.normal()
        x[k + 1] = x[k] + drift * dt + noise

    x_final = x[-1]
    p_accept = sigmoid(x_final - theta_readout)
    accept = int(p_accept >= 0.5)

    fixed_points = find_nullclines_1d(
        u=u,
        stim_on=stim_on,
        lam=lam_eff,
        a=a,
        gain=gain_eff,
        theta_dyn=theta_dyn,
        model=model,
    )

    stability = []
    for fp in fixed_points:
        label, deriv = classify_fixed_point_stability(
            fp,
            u=u,
            lam=lam_eff,
            a=a,
            gain=gain_eff,
            theta_dyn=theta_dyn,
            model=model,
        )
        stability.append(
            {
                "x_star": fp,
                "stability": label,
                "slope": deriv,
            }
        )

    return {
        "t": t,
        "x": x,
        "x_final": x_final,
        "p_accept": p_accept,
        "accept": accept,
        "fixed_points": fixed_points,
        "fixed_point_info": stability,
        "lam_eff": lam_eff,
        "sigma_eff": sigma_eff,
        "gain_eff": gain_eff,
    }


# =========================
# 8. Plot phase line / nullcline
# =========================

def plot_phase_line_and_nullcline(
    u,
    stim_on,
    lam=1.0,
    a=1.2,
    gain=1.5,
    theta_dyn=0.0,
    model="nonlinear_tanh",
    car_density=0.0,
    time_pressure=False,
    mode="manual",
    sigma=0.05,
    density_lam_gain=0.5,
    density_sigma_gain=1.5,
    time_gain_multiplier=1.5,
    auto_lam_multiplier=1.2,
    auto_sigma_multiplier=0.7,
    x_min=-5.0,
    x_max=5.0,
    n=600,
    ax=None,
):
    """
    Plot dx/dt vs x using context-adjusted parameters.
    """
    if ax is None:
        fig, ax = plt.subplots(figsize=(6, 4))

    lam_eff, _, gain_eff = apply_context_effects(
        lam=lam,
        sigma=sigma,
        gain=gain,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        density_lam_gain=density_lam_gain,
        density_sigma_gain=density_sigma_gain,
        time_gain_multiplier=time_gain_multiplier,
        auto_lam_multiplier=auto_lam_multiplier,
        auto_sigma_multiplier=auto_sigma_multiplier,
    )

    x_grid = np.linspace(x_min, x_max, n)
    dxdt = decision_drift(
        x_grid,
        u=u,
        stim_on=stim_on,
        lam=lam_eff,
        a=a,
        gain=gain_eff,
        theta_dyn=theta_dyn,
        model=model,
    )

    if model == "linear":
        label = r'$\dot{x}=-\lambda_{eff} x + u$'
    elif model == "nonlinear_tanh":
        label = r'$\dot{x}=-\lambda_{eff} x + a\tanh(g_{eff}(x-\theta_{dyn})) + u$'
    else:
        label = r'$\dot{x}=-\lambda_{eff} x + a(2\sigma(g_{eff}(x-\theta_{dyn}))-1) + u$'

    label = None

    ax.plot(x_grid, dxdt, color="blue", label=label)
    ax.axhline(0.0, linestyle="--", color="black", linewidth=1)

    if model != "linear":
        ax.axvline(
            theta_dyn,
            linestyle="-",
            color="green",
            linewidth=2.0,
            label=f"theta_dyn={theta_dyn:.2f}",
        )

    fixed_points = find_nullclines_1d(
        u=u,
        stim_on=stim_on,
        lam=lam_eff,
        a=a,
        gain=gain_eff,
        theta_dyn=theta_dyn,
        model=model,
        x_min=x_min,
        x_max=x_max,
        n=3000,
    )

    seen_stable = False
    seen_unstable = False

    for fp in fixed_points:
        stab, _ = classify_fixed_point_stability(
            fp,
            u=u,
            lam=lam_eff,
            a=a,
            gain=gain_eff,
            theta_dyn=theta_dyn,
            model=model,
        )
        ls = "-" if stab == "stable" else "--"

        label_fp = None
        if stab == "stable" and not seen_stable:
            label_fp = "stable fp"
            seen_stable = True
        elif stab == "unstable" and not seen_unstable:
            label_fp = "unstable fp"
            seen_unstable = True

        ax.axvline(fp, linestyle=ls, alpha=0.8, linewidth=1.2, color="red", label=label_fp)
        ax.text(
            fp,
            0.02 * (np.max(dxdt) - np.min(dxdt) + 1e-8),
            f"{fp:.2f}",
            rotation=0,
            va="bottom",
            ha="left",
            fontsize=8,
        )

    ax.set_xlabel("x")
    ax.set_ylabel("dx/dt")
    ax.set_title(f"1D Phase Line / Nullcline ({model})")
    ax.legend(bbox_to_anchor=(1.05, 0), loc="lower left", fontsize=8)

    return ax, fixed_points


# =========================
# 9. Plot trajectories from different initial states
# =========================

def plot_trajectories_different_x0(
    x0_list,
    u,
    stim_on,
    lam=1.0,
    a=1.2,
    gain=1.5,
    theta_dyn=0.0,
    car_density=0.0,
    time_pressure=False,
    mode="manual",
    sigma=0.0,
    dt=0.01,
    T=3.0,
    theta_readout=0.0,
    model="nonlinear_tanh",
    density_lam_gain=0.5,
    density_sigma_gain=1.5,
    time_gain_multiplier=1.5,
    auto_lam_multiplier=1.2,
    auto_sigma_multiplier=0.7,
):
    fig, axes = plt.subplots(1, 2, figsize=(15, 4))

    plot_phase_line_and_nullcline(
        u=u,
        stim_on=stim_on,
        lam=lam,
        a=a,
        gain=gain,
        theta_dyn=theta_dyn,
        model=model,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        sigma=sigma,
        density_lam_gain=density_lam_gain,
        density_sigma_gain=density_sigma_gain,
        time_gain_multiplier=time_gain_multiplier,
        auto_lam_multiplier=auto_lam_multiplier,
        auto_sigma_multiplier=auto_sigma_multiplier,
        ax=axes[0],
    )

    lam_eff, _, gain_eff = apply_context_effects(
        lam=lam,
        sigma=sigma,
        gain=gain,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        density_lam_gain=density_lam_gain,
        density_sigma_gain=density_sigma_gain,
        time_gain_multiplier=time_gain_multiplier,
        auto_lam_multiplier=auto_lam_multiplier,
        auto_sigma_multiplier=auto_sigma_multiplier,
    )

    for x0 in x0_list:
        dx0 = decision_drift(
            x0,
            u=u,
            stim_on=stim_on,
            lam=lam_eff,
            a=a,
            gain=gain_eff,
            theta_dyn=theta_dyn,
            model=model,
        )
        axes[0].scatter([x0], [dx0], s=50)

    all_fixed_points = None

    for i, x0 in enumerate(x0_list):
        result = simulate_decision_1d(
            x0=x0,
            u=u,
            stim_on=stim_on,
            lam=lam,
            a=a,
            gain=gain,
            theta_dyn=theta_dyn,
            car_density=car_density,
            time_pressure=time_pressure,
            mode=mode,
            sigma=sigma,
            dt=dt,
            T=T,
            theta_readout=theta_readout,
            model=model,
            random_state=100 + i,
            density_lam_gain=density_lam_gain,
            density_sigma_gain=density_sigma_gain,
            time_gain_multiplier=time_gain_multiplier,
            auto_lam_multiplier=auto_lam_multiplier,
            auto_sigma_multiplier=auto_sigma_multiplier,
        )
        all_fixed_points = result["fixed_points"]
        label = f"x0={x0:.2f}, final={result['x_final']:.2f}, accept={result['accept']}"
        axes[1].plot(result["t"], result["x"], label=label)
        axes[1].scatter(0, [x0], s=50)

    axes[1].axhline(
        theta_readout,
        linestyle="-",
        linewidth=2,
        color = 'green',
        label=f"theta_readout={theta_readout:.2f}",
    )

    if all_fixed_points is not None:
        for fp in all_fixed_points:
            stab, _ = classify_fixed_point_stability(
                fp,
                u=u,
                lam=lam_eff,
                a=a,
                gain=gain_eff,
                theta_dyn=theta_dyn,
                model=model,
            )
            ls = "-" if stab == "stable" else "--"
            axes[1].axhline(fp, linestyle=ls, linewidth=1.5, color="red", alpha=0.8, label=stab)

    axes[1].set_xlabel("time")
    axes[1].set_ylabel("x(t)")
    axes[1].set_title(f"Trajectories from Different Initial States ({model})")
    axes[1].legend(bbox_to_anchor=(1.05, 0), loc="lower left", fontsize=8)

    plt.tight_layout()
    plt.show()


# =========================
# 10. Single-trial demo
# =========================

def run_single_trial_demo(
    row,
    beta_c=1.2,
    beta_i=0.8,
    beta_ci=0.5,
    gamma_throttle=1.0,
    gamma_density=0.0,
    gamma_time=0.0,
    gamma_mode=0.0,
    kappa=1.0,
    lam=1.0,
    a=1.2,
    gain=1.5,
    theta_dyn=0.0,
    sigma=0.05,
    dt=0.01,
    T=5.0,
    theta_readout=0.0,
    model="nonlinear_tanh",
    density_lam_gain=0.5,
    density_sigma_gain=1.5,
    time_gain_multiplier=1.5,
    auto_lam_multiplier=1.2,
    auto_sigma_multiplier=0.7,
):
    p0 = float(row["subject_prior_accept_prob_subcategory"])
    coherence = float(row["coherence"])
    intensity = float(row["intensity"])
    throttle_pressure = float(row["var_throttle_pre2s"])
    car_density = float(row["car_density"])
    time_pressure = row["time_pressure"]
    mode = row["mode"]
    stim_on = stim_on_hill(
        beta_c * coherence
        + beta_i * intensity
        + beta_ci * coherence * intensity
    )

    x0 = compute_x0_from_prior(p0, kappa=kappa)

    u = compute_drive_u(
        coherence=coherence,
        intensity=intensity,
        throttle_pressure=throttle_pressure,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        beta_c=beta_c,
        beta_i=beta_i,
        beta_ci=beta_ci,
        gamma_throttle=gamma_throttle,
        gamma_density=gamma_density,
        gamma_time=gamma_time,
        gamma_mode=gamma_mode,
    )

    result = simulate_decision_1d(
        x0=x0,
        u=u,
        stim_on=stim_on,
        lam=lam,
        a=a,
        gain=gain,
        theta_dyn=theta_dyn,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        sigma=sigma,
        dt=dt,
        T=T,
        theta_readout=theta_readout,
        model=model,
        density_lam_gain=density_lam_gain,
        density_sigma_gain=density_sigma_gain,
        time_gain_multiplier=time_gain_multiplier,
        auto_lam_multiplier=auto_lam_multiplier,
        auto_sigma_multiplier=auto_sigma_multiplier,
    )

    print("===== Single Trial Demo (1D) =====")
    print(f"model = {model}")
    print(f"prior p0 = {p0:.3f}")
    print(f"x0 = {x0:.3f}")
    print(f"u = {u:.3f}")
    print(f"lam_eff = {result['lam_eff']:.3f}")
    print(f"sigma_eff = {result['sigma_eff']:.3f}")
    print(f"gain_eff = {result['gain_eff']:.3f}")
    print(f"theta_dyn = {theta_dyn:.3f}")
    print(f"theta_readout = {theta_readout:.3f}")
    print(f"x_final = {result['x_final']:.3f}")
    print(f"p_accept = {result['p_accept']:.3f}")
    print(f"predicted accept = {result['accept']}")

    if len(result["fixed_points"]) == 0:
        print("fixed points = none found in current plotting/search range")
    else:
        print("fixed points:")
        for item in result["fixed_point_info"]:
            print(
                f"  x* = {item['x_star']:.3f}, "
                f"{item['stability']}, slope={item['slope']:.3f}"
            )

    x0_list = [x0 - 2.0, x0 - 1.0, x0, x0 + 1.0, x0 + 2.0]
    plot_trajectories_different_x0(
        x0_list=x0_list,
        u=u,
        stim_on=stim_on,
        lam=lam,
        a=a,
        gain=gain,
        theta_dyn=theta_dyn,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        sigma=sigma,
        dt=dt,
        T=T,
        theta_readout=theta_readout,
        model=model,
        density_lam_gain=density_lam_gain,
        density_sigma_gain=density_sigma_gain,
        time_gain_multiplier=time_gain_multiplier,
        auto_lam_multiplier=auto_lam_multiplier,
        auto_sigma_multiplier=auto_sigma_multiplier,
    )

    return result


# =========================
# 11. Batch simulation on dataframe
# =========================

def simulate_dataframe_decisions(
    df,
    beta_c=1.2,
    beta_i=0.8,
    beta_ci=0.5,
    gamma_throttle=1.0,
    gamma_density=0.0,
    gamma_time=0.0,
    gamma_mode=0.0,
    kappa=1.0,
    lam=1.0,
    a=1.2,
    gain=1.5,
    theta_dyn=0.0,
    sigma=0.05,
    dt=0.01,
    T=5.0,
    theta_readout=0.0,
    model="nonlinear_tanh",
    density_lam_gain=0.5,
    density_sigma_gain=1.5,
    time_gain_multiplier=1.5,
    auto_lam_multiplier=1.2,
    auto_sigma_multiplier=0.7,
):
    records = []

    for idx, row in df.iterrows():
        p0 = float(row["subject_prior_accept_prob_subcategory"])
        coherence = float(row["coherence"])
        intensity = float(row["intensity"])
        throttle_pressure = float(row["var_throttle_pre2s"])
        car_density = float(row["car_density"])
        time_pressure = row["time_pressure"]
        mode = row["mode"]
        stim_on = stim_on_hill(
            beta_c * coherence
            + beta_i * intensity
            + beta_ci * coherence * intensity
        )

        x0 = compute_x0_from_prior(p0, kappa=kappa)

        u = compute_drive_u(
            coherence=coherence,
            intensity=intensity,
            throttle_pressure=throttle_pressure,
            car_density=car_density,
            time_pressure=time_pressure,
            mode=mode,
            beta_c=beta_c,
            beta_i=beta_i,
            beta_ci=beta_ci,
            gamma_throttle=gamma_throttle,
            gamma_density=gamma_density,
            gamma_time=gamma_time,
            gamma_mode=gamma_mode,
        )

        result = simulate_decision_1d(
            x0=x0,
            u=u,
            stim_on=stim_on,
            lam=lam,
            a=a,
            gain=gain,
            theta_dyn=theta_dyn,
            car_density=car_density,
            time_pressure=time_pressure,
            mode=mode,
            sigma=sigma,
            dt=dt,
            T=T,
            theta_readout=theta_readout,
            model=model,
            density_lam_gain=density_lam_gain,
            density_sigma_gain=density_sigma_gain,
            time_gain_multiplier=time_gain_multiplier,
            auto_lam_multiplier=auto_lam_multiplier,
            auto_sigma_multiplier=auto_sigma_multiplier,
        )

        fps = result["fixed_points"]
        if len(fps) == 0:
            n_fixed = 0
            fixed_points_str = ""
        else:
            n_fixed = len(fps)
            fixed_points_str = ",".join([f"{v:.4f}" for v in fps])

        records.append(
            {
                "idx": idx,
                "x0": x0,
                "u": u,
                "lam_eff": result["lam_eff"],
                "sigma_eff": result["sigma_eff"],
                "gain_eff": result["gain_eff"],
                "x_final": result["x_final"],
                "p_accept_pred": result["p_accept"],
                "accept_pred": result["accept"],
                "n_fixed_points": n_fixed,
                "fixed_points": fixed_points_str,
                "accept_true": int(row["accept"]) if "accept" in row and not pd.isna(row["accept"]) else np.nan,
            }
        )

    return pd.DataFrame(records)


# =========================
# 12. Convenience plotting for a row
# =========================

def inspect_single_trial_phase_portrait(
    row,
    beta_c=1.2,
    beta_i=0.8,
    beta_ci=0.5,
    gamma_throttle=1.0,
    gamma_density=0.0,
    gamma_time=0.0,
    gamma_mode=0.0,
    kappa=1.0,
    lam=1.0,
    a=1.2,
    gain=1.5,
    theta_dyn=0.0,
    sigma=0.05,
    model="nonlinear_tanh",
    density_lam_gain=0.5,
    density_sigma_gain=1.5,
    time_gain_multiplier=1.5,
    auto_lam_multiplier=1.2,
    auto_sigma_multiplier=0.7,
    x_min=-5.0,
    x_max=5.0,
):
    p0 = float(row["subject_prior_accept_prob_subcategory"])
    coherence = float(row["coherence"])
    intensity = float(row["intensity"])
    throttle_pressure = float(row["var_throttle_pre2s"])
    car_density = float(row["car_density"])
    time_pressure = row["time_pressure"]
    mode = row["mode"]
    stim_on = stim_on_hill(
        beta_c * coherence
        + beta_i * intensity
        + beta_ci * coherence * intensity
    )

    x0 = compute_x0_from_prior(p0, kappa=kappa)

    u = compute_drive_u(
        coherence=coherence,
        intensity=intensity,
        throttle_pressure=throttle_pressure,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        beta_c=beta_c,
        beta_i=beta_i,
        beta_ci=beta_ci,
        gamma_throttle=gamma_throttle,
        gamma_density=gamma_density,
        gamma_time=gamma_time,
        gamma_mode=gamma_mode,
    )

    lam_eff, _, gain_eff = apply_context_effects(
        lam=lam,
        sigma=sigma,
        gain=gain,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        density_lam_gain=density_lam_gain,
        density_sigma_gain=density_sigma_gain,
        time_gain_multiplier=time_gain_multiplier,
        auto_lam_multiplier=auto_lam_multiplier,
        auto_sigma_multiplier=auto_sigma_multiplier,
    )

    fig, ax = plt.subplots(figsize=(6, 4))
    plot_phase_line_and_nullcline(
        u=u,
        stim_on=stim_on,
        lam=lam,
        a=a,
        gain=gain,
        theta_dyn=theta_dyn,
        model=model,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        sigma=sigma,
        density_lam_gain=density_lam_gain,
        density_sigma_gain=density_sigma_gain,
        time_gain_multiplier=time_gain_multiplier,
        auto_lam_multiplier=auto_lam_multiplier,
        auto_sigma_multiplier=auto_sigma_multiplier,
        x_min=x_min,
        x_max=x_max,
        ax=ax,
    )

    dx0 = decision_drift(
        x0,
        u=u,
        stim_on=stim_on,
        lam=lam_eff,
        a=a,
        gain=gain_eff,
        theta_dyn=theta_dyn,
        model=model,
    )
    ax.scatter([x0], [dx0], s=60, label=f"x0={x0:.2f}")
    ax.legend(fontsize=8)
    plt.tight_layout()
    plt.show()


# =========================
# 13. Suggested default parameter regimes
# =========================

DEFAULT_LINEAR_PARAMS = dict(
    model="linear",
    lam=1.0,
    a=0.0,
    gain=1.5,
    theta_dyn=0.0,
    theta_readout=0.0,
)

DEFAULT_NONLINEAR_TANH_PARAMS = dict(
    model="nonlinear_tanh",
    lam=1.0,
    a=1.8,
    gain=2.0,
    theta_dyn=0.0,
    theta_readout=0.0,
)

DEFAULT_BISTABLE_TANH_PARAMS = dict(
    model="nonlinear_tanh",
    lam=1.0,
    a=2.4,
    gain=2.8,
    theta_dyn=0.0,
    theta_readout=0.0,
)

DEFAULT_NONLINEAR_SIGMOID_PARAMS = dict(
    model="nonlinear_sigmoid",
    lam=1.0,
    a=1.8,
    gain=2.0,
    theta_dyn=0.0,
    theta_readout=0.0,
)

# =============================
# return dynamical correction
# =============================

def build_dynamical_correction_features(
    df,
    beta_c=1.2,
    beta_i=0.8,
    beta_ci=0.5,
    gamma_throttle=1.0,
    gamma_density=0.0,
    gamma_time=0.0,
    gamma_mode=0.0,
    kappa=1.0,
    lam=1.0,
    a=1.8,
    gain=2.0,
    theta_dyn=0.0,
    sigma=0.0,
    dt=0.01,
    T=5.0,
    theta_readout=0.0,
):
    records = []

    for idx, row in df.iterrows():
        p0 = float(row["subject_prior_accept_prob_subcategory"])
        coherence = float(row["coherence"])
        intensity = float(row["intensity"])
        throttle_pressure = float(row["var_throttle_pre2s"])
        car_density = float(row["car_density"])
        time_pressure = row["time_pressure"]
        mode = row["mode"]
        stim_on = stim_on_hill(
            beta_c * coherence
            + beta_i * intensity
            + beta_ci * coherence * intensity
        )

        x0 = compute_x0_from_prior(p0, kappa=kappa, noise_std=0.0)

        u = compute_drive_u(
            coherence=coherence,
            intensity=intensity,
            throttle_pressure=throttle_pressure,
            car_density=car_density,
            time_pressure=time_pressure,
            mode=mode,
            beta_c=beta_c,
            beta_i=beta_i,
            beta_ci=beta_ci,
            gamma_throttle=gamma_throttle,
            gamma_density=gamma_density,
            gamma_time=gamma_time,
            gamma_mode=gamma_mode,
        )

        res_linear = simulate_decision_1d(
            x0=x0,
            u=u,
            stim_on=stim_on,
            lam=lam,
            a=0.0,
            gain=gain,
            theta_dyn=theta_dyn,
            car_density=car_density,
            time_pressure=time_pressure,
            mode=mode,
            sigma=sigma,
            dt=dt,
            T=T,
            theta_readout=theta_readout,
            model="linear",
            random_state=0,
        )

        res_nl = simulate_decision_1d(
            x0=x0,
            u=u,
            stim_on=stim_on,
            lam=lam,
            a=a,
            gain=gain,
            theta_dyn=theta_dyn,
            car_density=car_density,
            time_pressure=time_pressure,
            mode=mode,
            sigma=sigma,
            dt=dt,
            T=T,
            theta_readout=theta_readout,
            model="nonlinear_tanh",
            random_state=0,
        )

        xT_linear = res_linear["x_final"]
        xT_nl = res_nl["x_final"]

        records.append({
            "idx": idx,
            "x0": x0,
            "u": u,
            "xT_linear": xT_linear,
            "xT_nonlinear": xT_nl,
            "delta_from_prior": xT_nl - x0,
            "delta_nonlinear_extra": xT_nl - xT_linear,
            "accept_true": row["accept"] if "accept" in row else np.nan,
        })

    return pd.DataFrame(records)

# ==============================
# stim_on gating function
# ==============================
def stim_on_hill(stim_raw, K=0.1, n=2.0):
    s = np.maximum(stim_raw, 0.0)
    return (s**n) / (K**n + s**n + 1e-12)

def stim_on_clipped_linear(stim_raw, theta0=0.1, theta1=1.0):
    if theta1 <= theta0:
        raise ValueError("theta1 must be > theta0")
    return np.clip((stim_raw - theta0) / (theta1 - theta0), 0.0, 1.0)