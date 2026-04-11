import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

try:
    from scipy.optimize import root
    _HAS_SCIPY = True
except Exception:
    _HAS_SCIPY = False


# =========================================================
# 1. Utilities
# =========================================================

def safe_logit(p, eps=1e-6):
    p = np.clip(p, eps, 1 - eps)
    return np.log(p / (1 - p))


def sigmoid(z):
    return 1.0 / (1.0 + np.exp(-z))


def normalize_mode(mode):
    if pd.isna(mode):
        return "manual"
    m = str(mode).strip().lower()
    if m in ["auto", "autonomous"]:
        return "auto"
    return "manual"


def normalize_time_pressure(v):
    if isinstance(v, (bool, np.bool_)):
        return int(v)
    if pd.isna(v):
        return 0
    if isinstance(v, str):
        s = v.strip().lower()
        return int(s in ["1", "true", "yes", "y"])
    return int(bool(v))


def stim_on_hill(stim_raw, K=0.1, n=2.0):
    s = np.maximum(stim_raw, 0.0)
    return (s**n) / (K**n + s**n + 1e-12)


def stim_on_clipped_linear(stim_raw, theta0=0.1, theta1=1.0):
    if theta1 <= theta0:
        raise ValueError("theta1 must be > theta0")
    return np.clip((stim_raw - theta0) / (theta1 - theta0), 0.0, 1.0)


# =========================================================
# 2. Effective pressure / drives
# =========================================================

def compute_effective_pressure(
    throttle_pressure,
    car_density,
    time_pressure,
    mode,
    auto_pressure_reduction=0.6,
    density_pressure_gain=0.8,
    time_pressure_gain=0.8,
):
    mode_norm = normalize_mode(mode)
    tp = normalize_time_pressure(time_pressure)

    p_eff = float(throttle_pressure)

    if mode_norm == "auto":
        p_eff -= auto_pressure_reduction

    p_eff += density_pressure_gain * float(car_density)
    p_eff += time_pressure_gain * tp
    return p_eff


def compute_accept_reject_drives(
    prior_accept_prob,
    coherence,
    intensity,
    throttle_pressure,
    car_density,
    time_pressure,
    mode,

    auto_pressure_reduction=0.6,
    density_pressure_gain=0.8,
    time_pressure_gain=0.8,

    # accept drive
    b_x=0.0,
    alpha_prior=0.0,
    alpha_coherence=1.2,
    alpha_intensity=0.7,
    alpha_ci=0.4,

    # reject drive
    b_y=0.0,
    beta_prior=0.0,
    beta_pressure=1.0,
    beta_density=0.8,
    beta_time_pressure=0.8,
):
    lp = safe_logit(prior_accept_prob)
    C = float(coherence)
    I = float(intensity)
    D = float(car_density)
    TP = normalize_time_pressure(time_pressure)

    p_eff = compute_effective_pressure(
        throttle_pressure=throttle_pressure,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        auto_pressure_reduction=auto_pressure_reduction,
        density_pressure_gain=density_pressure_gain,
        time_pressure_gain=time_pressure_gain,
    )

    f_x = (
        b_x
        + alpha_prior * lp
        + alpha_coherence * C
        + alpha_intensity * I
        + alpha_ci * C * I
    )

    f_y = (
        b_y
        - beta_prior * lp
        + beta_pressure * p_eff
        + beta_density * D
        + beta_time_pressure * TP
    )

    return {
        "f_x": f_x,
        "f_y": f_y,
        "p_eff": p_eff,
        "prior_logit": lp,
    }


# =========================================================
# 3. Initial condition
# =========================================================

def default_initial_condition(
    p0,
    k_init=1.0,
    noise_std=0.02,
    rng=None,
    eps=1e-6,
):
    if rng is None:
        rng = np.random.default_rng()

    p = p0 + rng.normal(0, noise_std)
    p = np.clip(p, eps, 1 - eps)

    bias = p - 0.5
    x0 = k_init * bias
    y0 = -k_init * bias
    return x0, y0


def perturb_p0(p0, noise_std=0.1, rng=None):
    if rng is None:
        rng = np.random.default_rng()

    p0_new = p0 + rng.normal(0, noise_std)
    p0_new = np.clip(p0_new, 1e-4, 1 - 1e-4)
    return p0_new


def generate_init_points_from_p0(p0, k_init=1.0, noise_std=0.05, n_points=7, rng=None):
    if rng is None:
        rng = np.random.default_rng()

    init_points = []
    for _ in range(n_points):
        p0_i = perturb_p0(p0, noise_std=noise_std, rng=rng)
        x0_i, y0_i = default_initial_condition(p0_i, k_init=k_init, rng=rng)
        init_points.append((x0_i, y0_i))
    return init_points


# =========================================================
# 4. Unified nonlinear 2D dynamics
# =========================================================

def phi_tanh(z, gain=1.0):
    return np.tanh(gain * z)


def compute_stim_gates(
    coherence,
    intensity,
    alpha_coherence=1.2,
    alpha_intensity=0.7,
    alpha_ci=0.4,
    gate_func="hill",
    gate_kwargs=None,
    symmetric=False,
    y_gate_scale=1.0,
):
    if gate_kwargs is None:
        gate_kwargs = {}

    stim_raw = (
        alpha_coherence * float(coherence)
        + alpha_intensity * float(intensity)
        + alpha_ci * float(coherence) * float(intensity)
    )

    if gate_func == "hill":
        s = stim_on_hill(stim_raw, **gate_kwargs)
    elif gate_func == "clipped_linear":
        s = stim_on_clipped_linear(stim_raw, **gate_kwargs)
    elif gate_func == "identity":
        s = np.maximum(stim_raw, 0.0)
    else:
        raise ValueError("gate_func must be one of: hill, clipped_linear, identity")

    stim_on_x = s
    stim_on_y = y_gate_scale * s if symmetric else 1.0
    return stim_raw, stim_on_x, stim_on_y


def decision_drift_2d(
    x,
    y,
    f_x,
    f_y,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,
    stim_on_x=1.0,
    stim_on_y=1.0,
):
    """
    Unified drift:

        input_x = f_x + a_x * x - w_xy * y
        input_y = f_y + a_y * y - w_yx * x

        dx/dt = -lam_x * x + stim_on_x * tanh(gain_x * input_x)
        dy/dt = -lam_y * y + stim_on_y * tanh(gain_y * input_y)
    """
    input_x = f_x + a_x * x - w_xy * y
    input_y = f_y + a_y * y - w_yx * x

    dxdt = -lam_x * x + stim_on_x * phi_tanh(input_x, gain=gain_x)
    dydt = -lam_y * y + stim_on_y * phi_tanh(input_y, gain=gain_y)
    return dxdt, dydt


# =========================================================
# 5. Simulation
# =========================================================

def simulate_2d_decision_system(
    x0,
    y0,
    f_x,
    f_y,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,
    stim_on_x=1.0,
    stim_on_y=1.0,
    sigma_x=0.02,
    sigma_y=0.02,
    dt=0.01,
    T=3.0,
    decision_threshold=0.0,
    random_state=None,
):
    rng = np.random.default_rng(random_state)

    n_steps = int(T / dt) + 1
    t = np.linspace(0, T, n_steps)

    x = np.zeros(n_steps, dtype=float)
    y = np.zeros(n_steps, dtype=float)
    x[0], y[0] = x0, y0

    for k in range(n_steps - 1):
        dxdt, dydt = decision_drift_2d(
            x[k], y[k],
            f_x=f_x, f_y=f_y,
            lam_x=lam_x, lam_y=lam_y,
            w_xy=w_xy, w_yx=w_yx,
            a_x=a_x, a_y=a_y,
            gain_x=gain_x, gain_y=gain_y,
            stim_on_x=stim_on_x,
            stim_on_y=stim_on_y,
        )

        noise_x = sigma_x * np.sqrt(dt) * rng.normal()
        noise_y = sigma_y * np.sqrt(dt) * rng.normal()

        x[k + 1] = x[k] + dxdt * dt + noise_x
        y[k + 1] = y[k] + dydt * dt + noise_y

    diff = x - y
    diff_final = diff[-1]
    p_accept = sigmoid(diff_final - decision_threshold)
    accept_pred = int(diff_final > decision_threshold)

    return {
        "t": t,
        "x": x,
        "y": y,
        "diff": diff,
        "x_final": x[-1],
        "y_final": y[-1],
        "diff_final": diff_final,
        "p_accept": p_accept,
        "accept_pred": accept_pred,
    }


# =========================================================
# 6. Vector field / phase plane
# =========================================================

def compute_vector_field(
    X,
    Y,
    f_x,
    f_y,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,
    stim_on_x=1.0,
    stim_on_y=1.0,
):
    U, V = decision_drift_2d(
        X, Y,
        f_x=f_x, f_y=f_y,
        lam_x=lam_x, lam_y=lam_y,
        w_xy=w_xy, w_yx=w_yx,
        a_x=a_x, a_y=a_y,
        gain_x=gain_x, gain_y=gain_y,
        stim_on_x=stim_on_x,
        stim_on_y=stim_on_y,
    )
    return U, V


def plot_phase_plane(
    f_x,
    f_y,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,
    stim_on_x=1.0,
    stim_on_y=1.0,
    xlim=(-4, 4),
    ylim=(-4, 4),
    grid_n=21,
    contour_n=300,
    ax=None,
):
    if ax is None:
        fig, ax = plt.subplots(figsize=(6, 6))

    xs = np.linspace(xlim[0], xlim[1], grid_n)
    ys = np.linspace(ylim[0], ylim[1], grid_n)
    X, Y = np.meshgrid(xs, ys)

    U, V = compute_vector_field(
        X, Y,
        f_x=f_x, f_y=f_y,
        lam_x=lam_x, lam_y=lam_y,
        w_xy=w_xy, w_yx=w_yx,
        a_x=a_x, a_y=a_y,
        gain_x=gain_x, gain_y=gain_y,
        stim_on_x=stim_on_x,
        stim_on_y=stim_on_y,
    )
    ax.quiver(X, Y, U, V, angles="xy")

    xs_c = np.linspace(xlim[0], xlim[1], contour_n)
    ys_c = np.linspace(ylim[0], ylim[1], contour_n)
    Xc, Yc = np.meshgrid(xs_c, ys_c)

    Uc, Vc = compute_vector_field(
        Xc, Yc,
        f_x=f_x, f_y=f_y,
        lam_x=lam_x, lam_y=lam_y,
        w_xy=w_xy, w_yx=w_yx,
        a_x=a_x, a_y=a_y,
        gain_x=gain_x, gain_y=gain_y,
        stim_on_x=stim_on_x,
        stim_on_y=stim_on_y,
    )

    ax.contour(Xc, Yc, Uc, levels=[0], linewidths=2, colors="C0")
    ax.contour(Xc, Yc, Vc, levels=[0], linewidths=2, colors="C1")

    diag = np.linspace(max(xlim[0], ylim[0]), min(xlim[1], ylim[1]), 400)
    ax.plot(diag, diag, linestyle="--", color="C2")

    legend_handles = [
        Line2D([0], [0], color="C0", lw=2, label="dx/dt = 0"),
        Line2D([0], [0], color="C1", lw=2, label="dy/dt = 0"),
        Line2D([0], [0], color="C2", lw=1.5, linestyle="--", label="x = y boundary"),
    ]

    ax.set_xlim(xlim)
    ax.set_ylim(ylim)
    ax.set_xlabel("accept state x")
    ax.set_ylabel("reject state y")
    ax.set_title("2D nonlinear decision phase plane")
    ax.legend(handles=legend_handles, bbox_to_anchor=(1.05, 0), loc="lower left", fontsize=8)
    ax.set_aspect("equal", "box")
    return ax


# =========================================================
# 7. Nonlinear fixed points
# =========================================================

def find_fixed_point_2d(
    f_x,
    f_y,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,
    stim_on_x=1.0,
    stim_on_y=1.0,
    x0_guess=0.0,
    y0_guess=0.0,
):
    """
    Numerical fixed point solve for the SAME nonlinear system used in simulation.
    """
    if not _HAS_SCIPY:
        return np.nan, np.nan, False

    def fun(z):
        x, y = z
        dxdt, dydt = decision_drift_2d(
            x, y,
            f_x=f_x, f_y=f_y,
            lam_x=lam_x, lam_y=lam_y,
            w_xy=w_xy, w_yx=w_yx,
            a_x=a_x, a_y=a_y,
            gain_x=gain_x, gain_y=gain_y,
            stim_on_x=stim_on_x,
            stim_on_y=stim_on_y,
        )
        return np.array([dxdt, dydt], dtype=float)

    sol = root(fun, x0=np.array([x0_guess, y0_guess], dtype=float))
    if sol.success:
        return float(sol.x[0]), float(sol.x[1]), True
    return np.nan, np.nan, False


# =========================================================
# 8. Trajectory plotting
# =========================================================

def plot_trajectories_from_inits(
    init_points,
    f_x,
    f_y,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,
    stim_on_x=1.0,
    stim_on_y=1.0,
    sigma_x=0.02,
    sigma_y=0.02,
    dt=0.01,
    T=3.0,
    decision_threshold=0.0,
    xlim=(-4, 4),
    ylim=(-4, 4),
):
    fig, axes = plt.subplots(1, 2, figsize=(12, 4))

    plot_phase_plane(
        f_x=f_x,
        f_y=f_y,
        lam_x=lam_x,
        lam_y=lam_y,
        w_xy=w_xy,
        w_yx=w_yx,
        a_x=a_x,
        a_y=a_y,
        gain_x=gain_x,
        gain_y=gain_y,
        stim_on_x=stim_on_x,
        stim_on_y=stim_on_y,
        xlim=xlim,
        ylim=ylim,
        ax=axes[0],
    )

    xf, yf, ok = find_fixed_point_2d(
        f_x=f_x,
        f_y=f_y,
        lam_x=lam_x,
        lam_y=lam_y,
        w_xy=w_xy,
        w_yx=w_yx,
        a_x=a_x,
        a_y=a_y,
        gain_x=gain_x,
        gain_y=gain_y,
        stim_on_x=stim_on_x,
        stim_on_y=stim_on_y,
    )
    if ok:
        axes[0].scatter([xf], [yf], color="black", s=60, marker="X", label="fixed point")
        axes[0].legend(fontsize=8)

    for i, (x0, y0) in enumerate(init_points):
        res = simulate_2d_decision_system(
            x0=x0,
            y0=y0,
            f_x=f_x,
            f_y=f_y,
            lam_x=lam_x,
            lam_y=lam_y,
            w_xy=w_xy,
            w_yx=w_yx,
            a_x=a_x,
            a_y=a_y,
            gain_x=gain_x,
            gain_y=gain_y,
            stim_on_x=stim_on_x,
            stim_on_y=stim_on_y,
            sigma_x=sigma_x,
            sigma_y=sigma_y,
            dt=dt,
            T=T,
            decision_threshold=decision_threshold,
            random_state=100 + i,
        )
        color = "blue" if res["accept_pred"] else "red"
        axes[0].plot(res["x"], res["y"], color=color)
        axes[0].scatter([x0], [y0], s=25, color=color)
        axes[0].scatter([res["x_final"]], [res["y_final"]], s=30, color=color, marker="o")

        label = f"({x0:.2f},{y0:.2f}) -> pred={res['accept_pred']}"
        axes[1].plot(res["t"], res["diff"], label=label, color=color)

    axes[1].axhline(decision_threshold, linestyle="--", label="decision threshold")
    axes[1].set_xlabel("time")
    axes[1].set_ylabel("x(t) - y(t)")
    axes[1].set_title("Decision variable diff")
    axes[1].legend(bbox_to_anchor=(1.05, 0), loc="lower left", fontsize=8)

    plt.tight_layout()
    plt.show()


# =========================================================
# 9. Single-trial demo
# =========================================================

def run_single_trial_demo_2d(
    row,

    # drives
    auto_pressure_reduction=0.6,
    density_pressure_gain=0.8,
    time_pressure_gain=0.8,

    b_x=0.0,
    alpha_prior=0.0,
    alpha_coherence=1.2,
    alpha_intensity=0.7,
    alpha_ci=0.4,

    b_y=0.0,
    beta_prior=0.0,
    beta_pressure=1.0,
    beta_density=0.8,
    beta_time_pressure=0.8,

    # dynamics
    k_init=1.0,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,
    sigma_x=0.02,
    sigma_y=0.02,
    dt=0.01,
    T=3.0,
    decision_threshold=0.0,

    # stimulus gate
    gate_func="hill",
    gate_kwargs=None,
    symmetric_gate=False,
    y_gate_scale=1.0,
):
    p0 = float(row["subject_prior_accept_prob_subcategory"])
    coherence = float(row["coherence"])
    intensity = float(row["intensity"])
    throttle_pressure = float(row["var_throttle_pre2s"])
    car_density = float(row["car_density"])
    time_pressure = row["time_pressure"]
    mode = row["mode"]

    drives = compute_accept_reject_drives(
        prior_accept_prob=p0,
        coherence=coherence,
        intensity=intensity,
        throttle_pressure=throttle_pressure,
        car_density=car_density,
        time_pressure=time_pressure,
        mode=mode,
        auto_pressure_reduction=auto_pressure_reduction,
        density_pressure_gain=density_pressure_gain,
        time_pressure_gain=time_pressure_gain,
        b_x=b_x,
        alpha_prior=alpha_prior,
        alpha_coherence=alpha_coherence,
        alpha_intensity=alpha_intensity,
        alpha_ci=alpha_ci,
        b_y=b_y,
        beta_prior=beta_prior,
        beta_pressure=beta_pressure,
        beta_density=beta_density,
        beta_time_pressure=beta_time_pressure,
    )

    stim_raw, stim_on_x, stim_on_y = compute_stim_gates(
        coherence=coherence,
        intensity=intensity,
        alpha_coherence=alpha_coherence,
        alpha_intensity=alpha_intensity,
        alpha_ci=alpha_ci,
        gate_func=gate_func,
        gate_kwargs=gate_kwargs,
        symmetric=symmetric_gate,
        y_gate_scale=y_gate_scale,
    )

    x0, y0 = default_initial_condition(p0, k_init=k_init)

    res = simulate_2d_decision_system(
        x0=x0,
        y0=y0,
        f_x=drives["f_x"],
        f_y=drives["f_y"],
        lam_x=lam_x,
        lam_y=lam_y,
        w_xy=w_xy,
        w_yx=w_yx,
        a_x=a_x,
        a_y=a_y,
        gain_x=gain_x,
        gain_y=gain_y,
        stim_on_x=stim_on_x,
        stim_on_y=stim_on_y,
        sigma_x=sigma_x,
        sigma_y=sigma_y,
        dt=dt,
        T=T,
        decision_threshold=decision_threshold,
    )

    xf, yf, ok = find_fixed_point_2d(
        f_x=drives["f_x"],
        f_y=drives["f_y"],
        lam_x=lam_x,
        lam_y=lam_y,
        w_xy=w_xy,
        w_yx=w_yx,
        a_x=a_x,
        a_y=a_y,
        gain_x=gain_x,
        gain_y=gain_y,
        stim_on_x=stim_on_x,
        stim_on_y=stim_on_y,
        x0_guess=x0,
        y0_guess=y0,
    )

    print("===== Single Trial Demo (2D unified nonlinear) =====")
    print(f"prior_accept_prob = {p0:.3f}")
    print(f"coherence = {coherence:.3f}")
    print(f"intensity = {intensity:.3f}")
    print(f"stim_raw = {stim_raw:.3f}")
    print(f"stim_on_x = {stim_on_x:.3f}")
    print(f"stim_on_y = {stim_on_y:.3f}")
    print(f"effective_pressure = {drives['p_eff']:.3f}")
    print(f"accept drive f_x = {drives['f_x']:.3f}")
    print(f"reject drive f_y = {drives['f_y']:.3f}")
    print(f"initial state = ({x0:.3f}, {y0:.3f})")
    if ok:
        print(f"fixed point = ({xf:.3f}, {yf:.3f})")
    else:
        print("fixed point = not found")
    print(f"final state = ({res['x_final']:.3f}, {res['y_final']:.3f})")
    print(f"final diff x-y = {res['diff_final']:.3f}")
    print(f"p_accept = {res['p_accept']:.3f}")
    print(f"accept_pred = {res['accept_pred']}")

    init_points = generate_init_points_from_p0(
        p0,
        k_init=k_init,
        noise_std=0.5,
        n_points=5
    )

    plot_trajectories_from_inits(
        init_points=init_points,
        f_x=drives["f_x"],
        f_y=drives["f_y"],
        lam_x=lam_x,
        lam_y=lam_y,
        w_xy=w_xy,
        w_yx=w_yx,
        a_x=a_x,
        a_y=a_y,
        gain_x=gain_x,
        gain_y=gain_y,
        stim_on_x=stim_on_x,
        stim_on_y=stim_on_y,
        sigma_x=sigma_x,
        sigma_y=sigma_y,
        dt=dt,
        T=T,
        decision_threshold=decision_threshold,
    )

    return {
        "drives": drives,
        "simulation": res,
        "fixed_point": (xf, yf) if ok else (np.nan, np.nan),
        "stim_raw": stim_raw,
        "stim_on_x": stim_on_x,
        "stim_on_y": stim_on_y,
    }


# =========================================================
# 10. Batch simulation
# =========================================================

def simulate_dataframe_2d(
    df,

    auto_pressure_reduction=0.6,
    density_pressure_gain=0.8,
    time_pressure_gain=0.8,

    b_x=0.0,
    alpha_prior=0.0,
    alpha_coherence=1.2,
    alpha_intensity=0.7,
    alpha_ci=0.4,

    b_y=0.0,
    beta_prior=0.0,
    beta_pressure=1.0,
    beta_density=0.8,
    beta_time_pressure=0.8,

    k_init=1.0,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,
    sigma_x=0.02,
    sigma_y=0.02,
    dt=0.01,
    T=3.0,
    decision_threshold=0.0,

    gate_func="hill",
    gate_kwargs=None,
    symmetric_gate=False,
    y_gate_scale=1.0,
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

        drives = compute_accept_reject_drives(
            prior_accept_prob=p0,
            coherence=coherence,
            intensity=intensity,
            throttle_pressure=throttle_pressure,
            car_density=car_density,
            time_pressure=time_pressure,
            mode=mode,
            auto_pressure_reduction=auto_pressure_reduction,
            density_pressure_gain=density_pressure_gain,
            time_pressure_gain=time_pressure_gain,
            b_x=b_x,
            alpha_prior=alpha_prior,
            alpha_coherence=alpha_coherence,
            alpha_intensity=alpha_intensity,
            alpha_ci=alpha_ci,
            b_y=b_y,
            beta_prior=beta_prior,
            beta_pressure=beta_pressure,
            beta_density=beta_density,
            beta_time_pressure=beta_time_pressure,
        )

        stim_raw, stim_on_x, stim_on_y = compute_stim_gates(
            coherence=coherence,
            intensity=intensity,
            alpha_coherence=alpha_coherence,
            alpha_intensity=alpha_intensity,
            alpha_ci=alpha_ci,
            gate_func=gate_func,
            gate_kwargs=gate_kwargs,
            symmetric=symmetric_gate,
            y_gate_scale=y_gate_scale,
        )

        x0, y0 = default_initial_condition(p0, k_init=k_init)

        res = simulate_2d_decision_system(
            x0=x0,
            y0=y0,
            f_x=drives["f_x"],
            f_y=drives["f_y"],
            lam_x=lam_x,
            lam_y=lam_y,
            w_xy=w_xy,
            w_yx=w_yx,
            a_x=a_x,
            a_y=a_y,
            gain_x=gain_x,
            gain_y=gain_y,
            stim_on_x=stim_on_x,
            stim_on_y=stim_on_y,
            sigma_x=sigma_x,
            sigma_y=sigma_y,
            dt=dt,
            T=T,
            decision_threshold=decision_threshold,
            random_state=idx if isinstance(idx, (int, np.integer)) else None,
        )

        xf, yf, ok = find_fixed_point_2d(
            f_x=drives["f_x"],
            f_y=drives["f_y"],
            lam_x=lam_x,
            lam_y=lam_y,
            w_xy=w_xy,
            w_yx=w_yx,
            a_x=a_x,
            a_y=a_y,
            gain_x=gain_x,
            gain_y=gain_y,
            stim_on_x=stim_on_x,
            stim_on_y=stim_on_y,
            x0_guess=x0,
            y0_guess=y0,
        )

        rec = {
            "idx": idx,
            "prior_accept_prob": p0,
            "prior_logit": drives["prior_logit"],
            "coherence": coherence,
            "intensity": intensity,
            "throttle_pressure": throttle_pressure,
            "car_density": car_density,
            "time_pressure": normalize_time_pressure(time_pressure),
            "mode": normalize_mode(mode),
            "effective_pressure": drives["p_eff"],
            "f_x": drives["f_x"],
            "f_y": drives["f_y"],
            "stim_raw": stim_raw,
            "stim_on_x": stim_on_x,
            "stim_on_y": stim_on_y,
            "x0": x0,
            "y0": y0,
            "fixed_x": xf if ok else np.nan,
            "fixed_y": yf if ok else np.nan,
            "x_final": res["x_final"],
            "y_final": res["y_final"],
            "diff_final": res["diff_final"],
            "p_accept_pred": res["p_accept"],
            "accept_pred": res["accept_pred"],
        }

        if "accept" in row.index and not pd.isna(row["accept"]):
            rec["accept_true"] = int(row["accept"])

        records.append(rec)

    return pd.DataFrame(records)


# =========================================================
# 11. Dynamical correction features
# =========================================================

def build_dynamical_correction_features(
    df,

    auto_pressure_reduction=0.6,
    density_pressure_gain=0.8,
    time_pressure_gain=0.8,

    b_x=0.0,
    alpha_prior=0.0,
    alpha_coherence=1.2,
    alpha_intensity=0.7,
    alpha_ci=0.4,

    b_y=0.0,
    beta_prior=0.0,
    beta_pressure=1.0,
    beta_density=0.8,
    beta_time_pressure=0.8,

    k_init=1.0,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,
    sigma_x=0.0,
    sigma_y=0.0,
    dt=0.01,
    T=3.0,
    decision_threshold=0.0,

    gate_func="hill",
    gate_kwargs=None,
    symmetric_gate=False,
    y_gate_scale=1.0,
):
    """
    Build features analogous to the 1D version.

    Main correction:
        delta_nonlinear_extra = diff_final_nonlinear - diff_final_linear

    Also returns:
        delta_from_prior_diff = diff_final_nonlinear - (x0 - y0)
    """
    records = []

    for idx, row in df.iterrows():
        p0 = float(row["subject_prior_accept_prob_subcategory"])
        coherence = float(row["coherence"])
        intensity = float(row["intensity"])
        throttle_pressure = float(row["var_throttle_pre2s"])
        car_density = float(row["car_density"])
        time_pressure = row["time_pressure"]
        mode = row["mode"]

        drives = compute_accept_reject_drives(
            prior_accept_prob=p0,
            coherence=coherence,
            intensity=intensity,
            throttle_pressure=throttle_pressure,
            car_density=car_density,
            time_pressure=time_pressure,
            mode=mode,
            auto_pressure_reduction=auto_pressure_reduction,
            density_pressure_gain=density_pressure_gain,
            time_pressure_gain=time_pressure_gain,
            b_x=b_x,
            alpha_prior=alpha_prior,
            alpha_coherence=alpha_coherence,
            alpha_intensity=alpha_intensity,
            alpha_ci=alpha_ci,
            b_y=b_y,
            beta_prior=beta_prior,
            beta_pressure=beta_pressure,
            beta_density=beta_density,
            beta_time_pressure=beta_time_pressure,
        )

        stim_raw, stim_on_x, stim_on_y = compute_stim_gates(
            coherence=coherence,
            intensity=intensity,
            alpha_coherence=alpha_coherence,
            alpha_intensity=alpha_intensity,
            alpha_ci=alpha_ci,
            gate_func=gate_func,
            gate_kwargs=gate_kwargs,
            symmetric=symmetric_gate,
            y_gate_scale=y_gate_scale,
        )

        rng = np.random.default_rng(0)
        x0, y0 = default_initial_condition(p0, k_init=k_init, noise_std=0.0, rng=rng)

        # nonlinear system
        res_nl = simulate_2d_decision_system(
            x0=x0,
            y0=y0,
            f_x=drives["f_x"],
            f_y=drives["f_y"],
            lam_x=lam_x,
            lam_y=lam_y,
            w_xy=w_xy,
            w_yx=w_yx,
            a_x=a_x,
            a_y=a_y,
            gain_x=gain_x,
            gain_y=gain_y,
            stim_on_x=stim_on_x,
            stim_on_y=stim_on_y,
            sigma_x=sigma_x,
            sigma_y=sigma_y,
            dt=dt,
            T=T,
            decision_threshold=decision_threshold,
            random_state=0,
        )

        # matched linear baseline:
        # use same structure but remove tanh/self feedback and gates by setting
        # a_x=a_y=0, stim_on_x=stim_on_y=1, gain irrelevant,
        # and approximate tanh(z) ~ z in low-gain regime with small gain.
        # Simpler and cleaner: explicit linear simulation.
        res_lin = simulate_2d_decision_system_linear(
            x0=x0,
            y0=y0,
            f_x=drives["f_x"],
            f_y=drives["f_y"],
            lam_x=lam_x,
            lam_y=lam_y,
            w_xy=w_xy,
            w_yx=w_yx,
            sigma_x=sigma_x,
            sigma_y=sigma_y,
            dt=dt,
            T=T,
            decision_threshold=decision_threshold,
            random_state=0,
        )

        diff0 = x0 - y0
        diff_final_nl = res_nl["diff_final"]
        diff_final_lin = res_lin["diff_final"]

        records.append({
            "idx": idx,
            "x0": x0,
            "y0": y0,
            "diff0": diff0,
            "stim_raw": stim_raw,
            "stim_on_x": stim_on_x,
            "stim_on_y": stim_on_y,
            "x_final_linear": res_lin["x_final"],
            "y_final_linear": res_lin["y_final"],
            "diff_final_linear": diff_final_lin,
            "x_final_nonlinear": res_nl["x_final"],
            "y_final_nonlinear": res_nl["y_final"],
            "diff_final_nonlinear": diff_final_nl,
            "delta_from_prior_diff": diff_final_nl - diff0,
            "delta_nonlinear_extra": diff_final_nl - diff_final_lin,
            "accept_true": int(row["accept"]) if "accept" in row.index and not pd.isna(row["accept"]) else np.nan,
        })

    return pd.DataFrame(records)


# =========================================================
# 12. Explicit linear baseline for correction
# =========================================================

def simulate_2d_decision_system_linear(
    x0,
    y0,
    f_x,
    f_y,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    sigma_x=0.02,
    sigma_y=0.02,
    dt=0.01,
    T=3.0,
    decision_threshold=0.0,
    random_state=None,
):
    rng = np.random.default_rng(random_state)

    n_steps = int(T / dt) + 1
    t = np.linspace(0, T, n_steps)

    x = np.zeros(n_steps, dtype=float)
    y = np.zeros(n_steps, dtype=float)
    x[0], y[0] = x0, y0

    for k in range(n_steps - 1):
        dxdt = -lam_x * x[k] + f_x - w_xy * y[k]
        dydt = -lam_y * y[k] + f_y - w_yx * x[k]

        noise_x = sigma_x * np.sqrt(dt) * rng.normal()
        noise_y = sigma_y * np.sqrt(dt) * rng.normal()

        x[k + 1] = x[k] + dxdt * dt + noise_x
        y[k + 1] = y[k] + dydt * dt + noise_y

    diff = x - y
    diff_final = diff[-1]
    p_accept = sigmoid(diff_final - decision_threshold)
    accept_pred = int(diff_final > decision_threshold)

    return {
        "t": t,
        "x": x,
        "y": y,
        "diff": diff,
        "x_final": x[-1],
        "y_final": y[-1],
        "diff_final": diff_final,
        "p_accept": p_accept,
        "accept_pred": accept_pred,
    }