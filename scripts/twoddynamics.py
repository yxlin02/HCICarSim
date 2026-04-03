import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D


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


# =========================================================
# 2. Effective driving pressure
# =========================================================
# mean_throttle_input_zscore_pre2s is the base driving pressure
# auto mode reduces the effective pressure

def compute_effective_pressure(
    throttle_pressure,
    car_density,
    time_pressure,
    mode,
    auto_pressure_reduction=0.6,
    density_pressure_gain=0.8,
    time_pressure_gain=0.8,
):
    """
    Returns an effective pressure scalar used mainly for reject drive.

    throttle_pressure: mean_throttle_input_zscore_pre2s
    car_density: 0~1
    time_pressure: bool-like
    mode: auto/manual
    """
    mode_norm = normalize_mode(mode)
    tp = normalize_time_pressure(time_pressure)

    p_eff = float(throttle_pressure)

    # auto reduces burden
    if mode_norm == "auto":
        p_eff -= auto_pressure_reduction

    # dense traffic increases burden
    p_eff += density_pressure_gain * float(car_density)

    # time pressure increases burden
    p_eff += time_pressure_gain * tp

    return p_eff


# =========================================================
# 3. Construct accept / reject drives
# =========================================================

def compute_accept_reject_drives(
    prior_accept_prob,
    coherence,
    intensity,
    throttle_pressure,
    car_density,
    time_pressure,
    mode,

    # effective pressure parameters
    auto_pressure_reduction=0.6,
    density_pressure_gain=0.8,
    time_pressure_gain=0.8,

    # accept drive parameters
    b_x=0.0,
    alpha_coherence=1.2,
    alpha_intensity=0.7,
    alpha_ci=0.4,
    alpha_density=0.3,

    # reject drive parameters
    b_y=0.0,
    beta_pressure=1.0,
    beta_density=0.8,
    beta_time_pressure=0.8,
    beta_low_coherence=1.0,
):
    """
    x-channel: accept drive
    y-channel: reject / resistance drive
    """
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

    # Accept drive:
    # prior high -> more accept
    # coherence high -> more accept
    # intensity may help if recommendation is salient
    # density may mildly suppress acceptance
    f_x = (
        b_x
        + alpha_coherence * C
        + alpha_intensity * I
        + alpha_ci * C * I
        - alpha_density * D
    )

    # Reject drive:
    # effective pressure high -> more reject
    # high density -> more reject
    # time pressure -> more reject
    # low coherence -> more reject
    # high prior reduces reject
    f_y = (
        b_y
        + beta_pressure * p_eff
        + beta_density * D
        + beta_time_pressure * TP
        + beta_low_coherence * (1.0 - C)
    )

    f_y = 0.4 * f_y

    return {
        "f_x": f_x,
        "f_y": f_y,
        "p_eff": p_eff,
        "prior_logit": lp
    }


# =========================================================
# 4. 2D dynamical system
# =========================================================
# dx/dt = -lam_x * x + f_x - w_xy * y + noise_x
# dy/dt = -lam_y * y + f_y - w_yx * x + noise_y

def simulate_2d_decision_system(
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

    x = np.zeros(n_steps)
    y = np.zeros(n_steps)
    x[0] = x0
    y[0] = y0

    for k in range(n_steps - 1):
        noise_x = sigma_x * np.sqrt(dt) * rng.normal()
        noise_y = sigma_y * np.sqrt(dt) * rng.normal()

        dx = (-lam_x * x[k] + f_x - w_xy * y[k]) * dt + noise_x
        dy = (-lam_y * y[k] + f_y - w_yx * x[k]) * dt + noise_y

        x[k + 1] = x[k] + dx
        y[k + 1] = y[k] + dy

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
# 5. Nullclines and fixed point
# =========================================================

def x_nullcline(y, f_x, lam_x=1.0, w_xy=1.3):
    # dx/dt = 0 => x = (f_x - w_xy * y) / lam_x
    return (f_x - w_xy * y) / lam_x

def y_nullcline(x, f_y, lam_y=1.0, w_yx=1.3):
    # dy/dt = 0 => y = (f_y - w_yx * x) / lam_y
    return (f_y - w_yx * x) / lam_y

def fixed_point_2d(f_x, f_y, lam_x=1.0, lam_y=1.0, w_xy=1.3, w_yx=1.3):
    A = np.array([
        [lam_x, w_xy],
        [w_yx, lam_y]
    ], dtype=float)
    b = np.array([f_x, f_y], dtype=float)
    sol = np.linalg.solve(A, b)
    return sol[0], sol[1]


# =========================================================
# 6. Plot phase plane
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
    model="linear",

    # nonlinear_sigmoid params
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,

    # nonlinear_cubic params
    alpha_x=1.0,
    alpha_y=1.0,
    beta_x=1.0,
    beta_y=1.0,
):
    if model == "linear":
        U = -lam_x * X + f_x - w_xy * Y
        V = -lam_y * Y + f_y - w_yx * X

    elif model == "nonlinear_sigmoid":
        input_x = f_x + a_x * X - w_xy * Y
        input_y = f_y + a_y * Y - w_yx * X

        U = -lam_x * X + np.tanh(gain_x * input_x)
        V = -lam_y * Y + np.tanh(gain_y * input_y)

    elif model == "nonlinear_cubic":
        U = (
            -lam_x * X
            + alpha_x * X
            - beta_x * X**3
            + f_x
            - w_xy * Y
        )

        V = (
            -lam_y * Y
            + alpha_y * Y
            - beta_y * Y**3
            + f_y
            - w_yx * X
        )

    else:
        raise ValueError(f"Unknown model type: {model}")

    return U, V

def plot_phase_plane(
    f_x,
    f_y,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    xlim=(-4, 4),
    ylim=(-4, 4),
    grid_n=21,
    contour_n=300,
    model="linear",

    # nonlinear_sigmoid params
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,

    # nonlinear_cubic params
    alpha_x=1.0,
    alpha_y=1.0,
    beta_x=1.0,
    beta_y=1.0,

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
        model=model,
        a_x=a_x, a_y=a_y,
        gain_x=gain_x, gain_y=gain_y,
        alpha_x=alpha_x, alpha_y=alpha_y,
        beta_x=beta_x, beta_y=beta_y,
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
        model=model,
        a_x=a_x, a_y=a_y,
        gain_x=gain_x, gain_y=gain_y,
        alpha_x=alpha_x, alpha_y=alpha_y,
        beta_x=beta_x, beta_y=beta_y,
    )

    ax.contour(Xc, Yc, Uc, levels=[0], linewidths=2, colors="C0")
    ax.contour(Xc, Yc, Vc, levels=[0], linewidths=2, colors="C1")

    diag = np.linspace(max(xlim[0], ylim[0]), min(xlim[1], ylim[1]), 400)
    ax.plot(diag, diag, linestyle="--", color="C2")

    legend_handles = [
        Line2D([0], [0], color="C0", lw=2, label="dx/dt = 0"),
        Line2D([0], [0], color="C1", lw=2, label="dy/dt = 0"),
        Line2D([0], [0], color="C2", lw=1.5, linestyle="--", label="x = y decision boundary"),
    ]

    ax.set_xlim(xlim)
    ax.set_ylim(ylim)
    ax.set_xlabel("accept state x")
    ax.set_ylabel("reject state y")
    ax.set_title(f"2D Decision Dynamics Phase Plane ({model})")
    ax.legend(handles=legend_handles, bbox_to_anchor=(1.05, 0), loc='lower left', fontsize=8)

    return ax


# =========================================================
# 7. Plot trajectories from multiple initial conditions
# =========================================================

def plot_trajectories_from_inits(
    init_points,
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
    xlim=(-4, 4),
    ylim=(-4, 4),
    simulate_func=simulate_2d_decision_system,
    model="linear",
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,
    alpha_x=1.0,
    alpha_y=1.0,
    beta_x=1.0,
    beta_y=1.0,
):
    fig, axes = plt.subplots(1, 2, figsize=(12, 4))

    plot_phase_plane(
        f_x=f_x,
        f_y=f_y,
        lam_x=lam_x,
        lam_y=lam_y,
        w_xy=w_xy,
        w_yx=w_yx,
        xlim=xlim,
        ylim=ylim,
        model=model,
        a_x=a_x,
        a_y=a_y,
        gain_x=gain_x,
        gain_y=gain_y,
        alpha_x=alpha_x,
        alpha_y=alpha_y,
        beta_x=beta_x,
        beta_y=beta_y,
        ax=axes[0],
    )

    model_kwargs = get_model_kwargs(
        model=model,
        a_x=a_x,
        a_y=a_y,
        gain_x=gain_x,
        gain_y=gain_y,
        alpha_x=alpha_x,
        alpha_y=alpha_y,
        beta_x=beta_x,
        beta_y=beta_y,
    )

    for i, (x0, y0) in enumerate(init_points):
        res = simulate_func(
            x0=x0,
            y0=y0,
            f_x=f_x,
            f_y=f_y,
            lam_x=lam_x,
            lam_y=lam_y,
            w_xy=w_xy,
            w_yx=w_yx,
            sigma_x=sigma_x,
            sigma_y=sigma_y,
            dt=dt,
            T=T,
            decision_threshold=decision_threshold,
            random_state=100 + i,
            **model_kwargs,
        )
        color = "blue" if res["accept_pred"] else "red"
        axes[0].plot(res["x"], res["y"], color=color)
        axes[0].scatter([x0], [y0], s=25, color=color)
        axes[0].scatter([res["x_final"]], [res["y_final"]], s=25, color=color, marker="X")

    for i, (x0, y0) in enumerate(init_points):
        res = simulate_func(
            x0=x0,
            y0=y0,
            f_x=f_x,
            f_y=f_y,
            lam_x=lam_x,
            lam_y=lam_y,
            w_xy=w_xy,
            w_yx=w_yx,
            sigma_x=sigma_x,
            sigma_y=sigma_y,
            dt=dt,
            T=T,
            decision_threshold=decision_threshold,
            random_state=100 + i,
            **model_kwargs,
        )
        label = f"({x0:.1f},{y0:.1f}) -> pred={res['accept_pred']}"
        color = "blue" if res["accept_pred"] else "red"
        axes[1].plot(res["t"], res["diff"], label=label, color=color)

    axes[1].axhline(decision_threshold, linestyle="--", label="decision threshold")
    axes[1].set_xlabel("time")
    axes[1].set_ylabel("x(t) - y(t)")
    axes[1].set_title("Decision Variable Difference")
    axes[1].legend(bbox_to_anchor=(1.05, 0), loc='lower left', fontsize=8)
    axes[0].set_aspect('equal', 'box')

    plt.tight_layout()
    plt.show()


# =========================================================
# 8. Default initial condition
# =========================================================
# Use prior to set an initial bias:
#   x0 = +k_init * logit(prior)
#   y0 = -k_init * logit(prior)

def default_initial_condition(p0, k_init=1.0):
    bias = p0 - 0.5
    return k_init * bias, -k_init * bias


# =========================================================
# 9. Single-trial demo
# =========================================================

def run_single_trial_demo_2d(row,
                             # effective pressure params
                             auto_pressure_reduction=0.6,
                             density_pressure_gain=0.8,
                             time_pressure_gain=0.8,

                             # accept drive params
                             b_x=0.0,
                             alpha_coherence=1.2,
                             alpha_intensity=0.7,
                             alpha_ci=0.4,
                             alpha_density=0.3,

                             # reject drive params
                             b_y=0.0,
                             beta_pressure=1.0,
                             beta_density=0.8,
                             beta_time_pressure=0.8,
                             beta_low_coherence=1.0,

                             # dynamics params
                             k_init=1.0,
                             lam_x=1.0,
                             lam_y=1.0,
                             w_xy=1.3,
                             w_yx=1.3,
                             sigma_x=0.02,
                             sigma_y=0.02,
                             dt=0.01,
                             T=3.0,
                             decision_threshold=0.0,
                             **kwargs,):
    p0 = float(row["subject_prior_accept_prob_subcategory"])
    coherence = float(row["coherence"])
    intensity = float(row["intensity"])
    throttle_pressure = float(row["var_throttle_pre2s"])
    car_density = float(row["car_density"])
    time_pressure = row["time_pressure"]
    mode = row["mode"]

    model = kwargs.get("model", "linear")

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
        alpha_coherence=alpha_coherence,
        alpha_intensity=alpha_intensity,
        alpha_ci=alpha_ci,
        alpha_density=alpha_density,

        b_y=b_y,
        beta_pressure=beta_pressure,
        beta_density=beta_density,
        beta_time_pressure=beta_time_pressure,
        beta_low_coherence=beta_low_coherence,
    )

    f_x = drives["f_x"]
    f_y = drives["f_y"]
    p_eff = drives["p_eff"]

    x0, y0 = default_initial_condition(p0, k_init=k_init)

    if model == "nonlinear_sigmoid":
        simulate_func = simulate_2d_decision_system_nonlinear
    elif model == "nonlinear_cubic":        
        simulate_func = simulate_2d_decision_system_cubic
    elif model == "linear":
        simulate_func = simulate_2d_decision_system
    else:
        raise ValueError(f"Unknown model type: {model}")

    model_kwargs = get_model_kwargs(
        model=model,
        a_x=kwargs.get("a_x", 1.2),
        a_y=kwargs.get("a_y", 1.2),
        gain_x=kwargs.get("gain_x", 1.0),
        gain_y=kwargs.get("gain_y", 1.0),
        alpha_x=kwargs.get("alpha_x", 1.0),
        alpha_y=kwargs.get("alpha_y", 1.0),
        beta_x=kwargs.get("beta_x", 1.0),
        beta_y=kwargs.get("beta_y", 1.0),
    )

    res = simulate_func(
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
        **model_kwargs,
    )

    xf, yf = fixed_point_2d(
        f_x=f_x,
        f_y=f_y,
        lam_x=lam_x,
        lam_y=lam_y,
        w_xy=w_xy,
        w_yx=w_yx
    )

    print("===== Single Trial Demo (2D) =====")
    print(f"prior_accept_prob = {p0:.3f}")
    print(f"coherence = {coherence:.3f}")
    print(f"intensity = {intensity:.3f}")
    print(f"throttle_pressure = {throttle_pressure:.3f}")
    print(f"car_density = {car_density:.3f}")
    print(f"time_pressure = {normalize_time_pressure(time_pressure)}")
    print(f"mode = {normalize_mode(mode)}")
    print(f"effective_pressure = {p_eff:.3f}")
    print(f"accept drive f_x = {f_x:.3f}")
    print(f"reject drive f_y = {f_y:.3f}")
    print(f"initial state = ({x0:.3f}, {y0:.3f})")
    print(f"fixed point = ({xf:.3f}, {yf:.3f})")
    print(f"final state = ({res['x_final']:.3f}, {res['y_final']:.3f})")
    print(f"final diff x-y = {res['diff_final']:.3f}")
    print(f"p_accept = {res['p_accept']:.3f}")
    print(f"accept_pred = {res['accept_pred']}")

    init_points = [
        (x0, y0),
        (x0 - 1.0, y0),
        (x0 + 1.0, y0),
        (x0, y0 - 1.0),
        (x0, y0 + 1.0),
        (x0 - 1.0, y0 - 1.0),
        (x0 + 1.0, y0 + 1.0),
    ]

    plot_trajectories_from_inits(
        init_points=init_points,
        f_x=f_x,
        f_y=f_y,
        lam_x=lam_x,
        lam_y=lam_y,
        w_xy=w_xy,
        w_yx=w_yx,
        sigma_x=sigma_x,
        sigma_y=sigma_y,
        dt=dt,
        T=T,
        decision_threshold=decision_threshold,
        simulate_func=simulate_func,
        model=model,
    )

    return {
        "drives": drives,
        "simulation": res,
        "fixed_point": (xf, yf)
    }


# =========================================================
# 10. Batch simulation for dataframe
# =========================================================

def simulate_dataframe_2d(
    df,
    # effective pressure params
    auto_pressure_reduction=0.6,
    density_pressure_gain=0.8,
    time_pressure_gain=0.8,

    # accept drive params
    b_x=0.0,
    alpha_coherence=1.2,
    alpha_intensity=0.7,
    alpha_ci=0.4,
    alpha_density=0.3,

    # reject drive params
    b_y=0.0,
    beta_pressure=1.0,
    beta_density=0.8,
    beta_time_pressure=0.8,
    beta_low_coherence=1.0,

    # dynamics
    k_init=1.0,
    lam_x=1.0,
    lam_y=1.0,
    w_xy=1.3,
    w_yx=1.3,
    sigma_x=0.02,
    sigma_y=0.02,
    dt=0.01,
    T=3.0,
    decision_threshold=0.0,

    **kwargs
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

        model = kwargs.get("model", "linear")

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
            alpha_coherence=alpha_coherence,
            alpha_intensity=alpha_intensity,
            alpha_ci=alpha_ci,
            alpha_density=alpha_density,

            b_y=b_y,
            beta_pressure=beta_pressure,
            beta_density=beta_density,
            beta_time_pressure=beta_time_pressure,
            beta_low_coherence=beta_low_coherence,
        )

        x0, y0 = default_initial_condition(p0, k_init=k_init)

        if model == "nonlinear_sigmoid":
            simulate_func = simulate_2d_decision_system_nonlinear
        elif model == "nonlinear_cubic":        
            simulate_func = simulate_2d_decision_system_cubic
        elif model == "linear":
            simulate_func = simulate_2d_decision_system
        else:
            raise ValueError(f"Unknown model type: {model}")
        
        model_kwargs = get_model_kwargs(
                model=model,
                a_x=kwargs.get("a_x", 1.2),
                a_y=kwargs.get("a_y", 1.2),
                gain_x=kwargs.get("gain_x", 1.0),
                gain_y=kwargs.get("gain_y", 1.0),
                alpha_x=kwargs.get("alpha_x", 1.0),
                alpha_y=kwargs.get("alpha_y", 1.0),
                beta_x=kwargs.get("beta_x", 1.0),
                beta_y=kwargs.get("beta_y", 1.0),
            )
        
        res = simulate_func(
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
            random_state=idx,
            **model_kwargs,
        )

        try:
            xf, yf = fixed_point_2d(
                f_x=drives["f_x"],
                f_y=drives["f_y"],
                lam_x=lam_x,
                lam_y=lam_y,
                w_xy=w_xy,
                w_yx=w_yx
            )
        except np.linalg.LinAlgError:
            xf, yf = np.nan, np.nan

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
            "x0": x0,
            "y0": y0,
            "fixed_x": xf,
            "fixed_y": yf,
            "x_final": res["x_final"],
            "y_final": res["y_final"],
            "diff_final": res["diff_final"],
            "p_accept_pred": res["p_accept"],
            "accept_pred": res["accept_pred"],
        }

        if "accept" in row.index:
            rec["accept_true"] = int(row["accept"])

        records.append(rec)

    return pd.DataFrame(records)

# ========================================================= 
# NonLinear 2D Dynamics for Human-AI Shared Control in Driving  
# =========================================================
def simulate_2d_decision_system_nonlinear(
    x0, y0, f_x, f_y,
    lam_x=1.0, lam_y=1.0,
    w_xy=1.3, w_yx=1.3,
    a_x=1.2, a_y=1.2,       # self-excitation
    gain_x=1.0, gain_y=1.0, # nonlinearity steepness
    sigma_x=0.02, sigma_y=0.02,
    dt=0.01, T=3.0,
    decision_threshold=0.0,
    random_state=None,
):
    rng = np.random.default_rng(random_state)

    def phi_x(z):
        return np.tanh(gain_x * z)

    def phi_y(z):
        return np.tanh(gain_y * z)

    n_steps = int(T / dt) + 1
    t = np.linspace(0, T, n_steps)

    x = np.zeros(n_steps)
    y = np.zeros(n_steps)
    x[0], y[0] = x0, y0

    for k in range(n_steps - 1):
        noise_x = sigma_x * np.sqrt(dt) * rng.normal()
        noise_y = sigma_y * np.sqrt(dt) * rng.normal()

        input_x = f_x + a_x * x[k] - w_xy * y[k]
        input_y = f_y + a_y * y[k] - w_yx * x[k]

        dx = (-lam_x * x[k] + phi_x(input_x)) * dt + noise_x
        dy = (-lam_y * y[k] + phi_y(input_y)) * dt + noise_y

        x[k + 1] = x[k] + dx
        y[k + 1] = y[k] + dy

    diff = x - y
    diff_final = diff[-1]
    p_accept = 1 / (1 + np.exp(-(diff_final - decision_threshold)))
    accept_pred = int(diff_final > decision_threshold)

    return {
        "t": t, "x": x, "y": y, "diff": diff,
        "x_final": x[-1], "y_final": y[-1],
        "diff_final": diff_final,
        "p_accept": p_accept,
        "accept_pred": accept_pred,
    }

def simulate_2d_decision_system_cubic(
    x0, y0, f_x, f_y,
    lam_x=0.0, lam_y=0.0,
    w_xy=1.3, w_yx=1.3,
    alpha_x=1.0, alpha_y=1.0,   # linear self-excitation
    beta_x=1.0, beta_y=1.0,     # cubic saturation
    sigma_x=0.02, sigma_y=0.02,
    dt=0.01, T=3.0,
    decision_threshold=0.0,
    random_state=None,
):
    rng = np.random.default_rng(random_state)

    n_steps = int(T / dt) + 1
    t = np.linspace(0, T, n_steps)

    x = np.zeros(n_steps)
    y = np.zeros(n_steps)
    x[0], y[0] = x0, y0

    for k in range(n_steps - 1):
        noise_x = sigma_x * np.sqrt(dt) * rng.normal()
        noise_y = sigma_y * np.sqrt(dt) * rng.normal()

        dx = (
            -lam_x * x[k]
            + alpha_x * x[k]
            - beta_x * x[k]**3
            + f_x
            - w_xy * y[k]
        ) * dt + noise_x

        dy = (
            -lam_y * y[k]
            + alpha_y * y[k]
            - beta_y * y[k]**3
            + f_y
            - w_yx * x[k]
        ) * dt + noise_y

        x[k + 1] = x[k] + dx
        y[k + 1] = y[k] + dy

    diff = x - y
    diff_final = diff[-1]
    p_accept = 1 / (1 + np.exp(-(diff_final - decision_threshold)))
    accept_pred = int(diff_final > decision_threshold)

    return {
        "t": t, "x": x, "y": y, "diff": diff,
        "x_final": x[-1], "y_final": y[-1],
        "diff_final": diff_final,
        "p_accept": p_accept,
        "accept_pred": accept_pred,
    }

# =========================================================
# model kwargs helper
# =========================================================
def get_model_kwargs(
    model,
    a_x=1.2,
    a_y=1.2,
    gain_x=1.0,
    gain_y=1.0,
    alpha_x=1.0,
    alpha_y=1.0,
    beta_x=1.0,
    beta_y=1.0,
):
    if model == "nonlinear_sigmoid":
        return {
            "a_x": a_x,
            "a_y": a_y,
            "gain_x": gain_x,
            "gain_y": gain_y,
        }
    elif model == "nonlinear_cubic":
        return {
            "alpha_x": alpha_x,
            "alpha_y": alpha_y,
            "beta_x": beta_x,
            "beta_y": beta_y,
        }
    elif model == "linear":
        return {}
    else:
        raise ValueError(f"Unknown model type: {model}")
    
# =========================================================
# optimization of parameters based on data
# =========================================================

def compute_objective(valid, prob_col="p_accept", pred_col="accept_pred", true_col="accept_true"):
    y_true = valid[true_col].astype(int).values
    y_pred = valid[pred_col].astype(int).values

    # accuracy
    acc = (y_true == y_pred).mean()

    # balanced accuracy
    pos_mask = (y_true == 1)
    neg_mask = (y_true == 0)

    tpr = (y_pred[pos_mask] == 1).mean() if pos_mask.any() else 0.5
    tnr = (y_pred[neg_mask] == 0).mean() if neg_mask.any() else 0.5
    bal_acc = 0.5 * (tpr + tnr)

    # prediction bias penalty
    pred_rate = y_pred.mean()
    true_rate = y_true.mean()
    rate_penalty = abs(pred_rate - true_rate)

    # optional probability calibration penalty
    if prob_col in valid.columns:
        p = np.clip(valid[prob_col].values, 1e-6, 1 - 1e-6)
        brier = np.mean((p - y_true) ** 2)
    else:
        brier = 0.0

    score = (
        0.55 * bal_acc
        + 0.25 * acc
        - 0.15 * rate_penalty
        - 0.05 * brier
    )

    return {
        "score": score,
        "acc": acc,
        "bal_acc": bal_acc,
        "pred_rate": pred_rate,
        "true_rate": true_rate,
        "rate_penalty": rate_penalty,
        "brier": brier,
    }

def sample_params(rng):
    return {
        "auto_pressure_reduction": rng.uniform(0.2, 1.0),
        "density_pressure_gain": rng.uniform(0.1, 1.2),
        "time_pressure_gain": rng.uniform(0.2, 2.0),

        "alpha_coherence": rng.uniform(1.0, 2.5),
        "alpha_intensity": rng.uniform(0.2, 1.0),
        "alpha_ci": rng.uniform(0.2, 1.5),
        "alpha_density": rng.uniform(0.0, 1.0),

        "beta_pressure": rng.uniform(0.1, 1.5),
        "beta_density": rng.uniform(0.1, 1.2),
        "beta_time_pressure": rng.uniform(0.2, 2.5),
        "beta_low_coherence": rng.uniform(0.1, 1.5),

        "lam_x": rng.uniform(0.6, 1.6),
        "lam_y": rng.uniform(0.6, 1.6),
        "w_xy": rng.uniform(0.5, 4.0),
        "w_yx": rng.uniform(0.5, 4.0),

        "sigma_x": rng.uniform(0.01, 0.08),
        "sigma_y": rng.uniform(0.01, 0.08),

        "T": rng.uniform(5.0, 12.0),
        "decision_threshold": 0.0,
        "model": "nonlinear_sigmoid",
    }

def evaluate_one_setting(df_reaction, params, twoddynamics, random_state=42):
    try:
        df_sim = twoddynamics.simulate_dataframe_2d(
            df_reaction,
            **params,
        )

        valid = df_sim.dropna(subset=["accept_true"]).copy()
        if len(valid) == 0:
            return None

        metrics = compute_objective(valid)
        result = {**params, **metrics, "n_valid": len(valid)}
        return result

    except Exception as e:
        return {"error": str(e), **params}
    
def random_search_twod(df_reaction, twoddynamics, n_iter=200, seed=42):
    rng = np.random.default_rng(seed)
    results = []

    for i in range(n_iter):
        params = sample_params(rng)
        res = evaluate_one_setting(df_reaction, params, twoddynamics, random_state=seed + i)
        if res is not None and "score" in res:
            results.append(res)

        if (i + 1) % 20 == 0:
            tmp = pd.DataFrame(results)
            if len(tmp) > 0:
                best_idx = tmp["score"].idxmax()
                print(f"[{i+1}/{n_iter}] best score = {tmp.loc[best_idx, 'score']:.4f}, "
                      f"acc = {tmp.loc[best_idx, 'acc']:.4f}, "
                      f"bal_acc = {tmp.loc[best_idx, 'bal_acc']:.4f}")

    return pd.DataFrame(results).sort_values("score", ascending=False).reset_index(drop=True)