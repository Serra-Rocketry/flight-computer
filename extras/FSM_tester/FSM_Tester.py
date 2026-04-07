import numpy as np
import matplotlib.pyplot as plt
import pandas as pd

# file = "Open csv file/dados_simulados.csv"
file = "Open csv file/13_30_11-Dados.csv"
# file = "Open csv file/dados_filtrados.csv"

df = pd.read_csv(file)
# from "# Time (s), Z (m), Ax (m/s²), Ay (m/s²), Az (m/s²)" to "millis,altp,ax,ay,az"

if file == "Open csv file/dados_simulados.csv": 
    df["altp"] = df["altp"] - df["altp"].iloc[0]   # Convert to altitude above ground level

if file == "Open csv file/13_30_11-Dados.csv":
    df["millis"] = df["millis"] / 1000.0  # Convert milliseconds to seconds

def detect_motor_burnout(_pressure, height, state_vector, u_dot):
    """Detect motor burnout by sudden drop in acceleration.

    Returns True when vertical acceleration becomes significantly negative
    (indicating end of propulsion phase) OR when total acceleration drops below
    a threshold indicating coasting/free-fall has begun.
    """
    try:
        if u_dot is None or len(u_dot) < 6:
            return False

        ax = float(u_dot[3])
        ay = float(u_dot[4])
        az = float(u_dot[5])

        # Defensive checks for NaN/Inf
        if not all(np.isfinite([ax, ay, az])):
            return False

        total_acc = np.sqrt(ax * ax + ay * ay + az * az)
        if not np.isfinite(total_acc):
            return False

        # Additional safety: ignore spurious low-accel readings at t~0 by
        # requiring the rocket to be above a small altitude and still ascending
        vz = float(state_vector[5]) if len(state_vector) > 5 else 0
        if not np.isfinite(vz):
            return False

        if height < 5.0 or vz <= 0.5:
            return False

        # Burnout detected when:
        # 1. Vertical acceleration becomes very negative (end of thrust phase)
        # 2. OR total acceleration drops below 2.0 m/s² (coasting detected)
        return az < -8.0 or total_acc < 2.0
    except (ValueError, TypeError, IndexError):
        return False


def detect_apogee_acceleration(_pressure, _height, state_vector, u_dot):
    """Detect apogee using near-zero vertical velocity and negative vertical accel.

    Apogee occurs when the rocket reaches its highest point, characterized by
    vertical velocity approaching zero and negative (downward) acceleration.
    """
    try:
        if state_vector is None or u_dot is None:
            return False
        if len(state_vector) < 6 or len(u_dot) < 6:
            return False

        vz = float(state_vector[5])
        az = float(u_dot[5])
        if not all(np.isfinite([vz, az])):
            return False

        # Slightly more permissive thresholds to avoid spurious misses
        return abs(vz) < 1.0 and az < -0.1
    except (ValueError, TypeError, IndexError):
        return False


def detect_freefall(_pressure, height, state_vector, u_dot):
    """Detect free-fall when total acceleration magnitude is low.

    Free-fall is characterized by acceleration magnitude close to gravitational
    acceleration (approximately -g in the vertical direction), or when the rocket
    is in a ballistic coasting phase with minimal thrust or drag effects.
    """
    try:
        if u_dot is None or len(u_dot) < 6:
            return False

        ax = float(u_dot[3])
        ay = float(u_dot[4])
        az = float(u_dot[5])
        if not all(np.isfinite([ax, ay, az])):
            return False

        total_acc = np.sqrt(ax * ax + ay * ay + az * az)
        if not np.isfinite(total_acc):
            return False

        # Require the rocket to be descending and above a small altitude to
        # avoid false positives before launch.
        vz = float(state_vector[5]) if len(state_vector) > 5 else 0
        if not np.isfinite(vz):
            return False

        if height < 5.0 or vz >= -0.2:
            return False

        # More sensitive threshold: detect free-fall at lower acceleration
        return total_acc < 11.5 and vz < -5
    except (ValueError, TypeError, IndexError):
        return False


def detect_liftoff(_pressure, _height, _state_vector, u_dot):
    """Detect liftoff by high total acceleration.

    Liftoff is characterized by a sudden increase in acceleration as the motor
    ignites and begins producing thrust.
    """
    try:
        if u_dot is None or len(u_dot) < 6:
            return False

        ax = float(u_dot[3])
        ay = float(u_dot[4])
        az = float(u_dot[5])
        if not all(np.isfinite([ax, ay, az])):
            return False

        total_acc = np.sqrt(ax * ax + ay * ay + az * az)
        if not np.isfinite(total_acc):
            return False

        return total_acc > 15.0
    except (ValueError, TypeError, IndexError):
        return False


def altitude_trigger_factory(target_altitude, require_descent=True):
    """Return a trigger that deploys when altitude <= target_altitude.

    If require_descent is True, also require vertical velocity negative
    (descending) to avoid firing during ascent.
    """

    def trigger(_pressure, height, state_vector, _u_dot=None):
        vz = float(state_vector[5])
        if require_descent:
            return (height <= target_altitude) and (vz < 0)
        return height <= target_altitude

    return trigger


PARACHUTE_ALTITUDE = 100
parachute_trigger = altitude_trigger_factory(PARACHUTE_ALTITUDE)

# Initialize variables
liftoff = False
burnout = False
apogee = False
freefall = False
parachute = False

df["vz"] = 0.0
df["event"] = ""

def smooth(value, prev_value, alpha):
    if prev_value is None:
        return value
    return alpha * value + (1 - alpha) * prev_value

# Process events
for i, row in df.iterrows():
    height = float(row["altp"])
    millis = float(row["millis"])
    # height = smooth(height, df.loc[i - 1, "altp"] if i > 0 else None, alpha=0.2)

    ax = float(row["ax"])
    ay = float(row["ay"])
    az = float(row["az"])
    # ax = smooth(ax, df.loc[i - 1, "ax"] if i > 0 else None, alpha=0.2)
    # ay = smooth(ay, df.loc[i - 1, "ay"] if i > 0 else None, alpha=0.2)
    # az = smooth(az, df.loc[i - 1, "az"] if i > 0 else None, alpha=0.2)

    # Estimate vertical velocity
    if i == 0:
        vz = 0.0
    else:
        prev_alt = float(df.iloc[i - 1]["altp"])
        prev_time = float(df.iloc[i - 1]["millis"])
        dt = (millis - prev_time)

        if dt <= 0:
            vz = 0.0
        else:
            vz = (height - prev_alt) / dt
            vz = np.clip(vz, -200, 200)

    df.loc[i, "vz"] = vz
    state_vector = [0, 0, height, 0, 0, vz]
    u_dot = [0, 0, 0, ax, ay, az]

    if not liftoff and detect_liftoff(None, height, state_vector, u_dot):
        df.loc[i, "event"] = "liftoff"
        print(
                f"LIFTOFF detectado em {millis:.2f} s | altura {height:.2f} | vz {vz:.2f} | ax {ax:.2f} | ay {ay:.2f} | az {az:.2f} | acc {np.sqrt(ax*ax + ay*ay + az*az):.2f}"
            )
        liftoff = True
    elif not burnout and detect_motor_burnout(None, height, state_vector, u_dot) and liftoff:
        df.loc[i, "event"] = "burnout"
        print(
                f"BURNOUT detectado em {millis:.2f} s | altura {height:.2f} | vz {vz:.2f} | ax {ax:.2f} | ay {ay:.2f} | az {az:.2f} | acc {np.sqrt(ax*ax + ay*ay + az*az):.2f}"
            )
        burnout = True
    elif not apogee and detect_apogee_acceleration(None, height, state_vector, u_dot) and burnout:
        df.loc[i, "event"] = "apogee"
        print(
                f"APOGEE detectado em {millis:.2f} s | altura {height:.2f} | vz {vz:.2f} | ax {ax:.2f} | ay {ay:.2f} | az {az:.2f} | acc {np.sqrt(ax*ax + ay*ay + az*az):.2f}"
            )
        apogee = True
    elif not freefall and detect_freefall(None, height, state_vector, u_dot) and apogee:
        df.loc[i, "event"] = "freefall"
        print(
                f"FREEFALL detectado em {millis:.2f} s | altura {height:.2f} | vz {vz:.2f} | ax {ax:.2f} | ay {ay:.2f} | az {az:.2f} | acc {np.sqrt(ax*ax + ay*ay + az*az):.2f}"
            )
        freefall = True
    # elif not parachute and parachute_trigger(None, height, state_vector) and freefall:
    #     df.loc[i, "event"] = "parachute"
    #     print(
    #             f"PARACHUTE trigger em {millis:.2f} s | altura {height:.2f} | vz {vz:.2f} | ax {ax:.2f} | ay {ay:.2f} | az {az:.2f} | acc {np.sqrt(ax*ax + ay*ay + az*az):.2f}"
    #         )
    #     parachute = True

# Plot
plt.figure(figsize=(12, 6))
plt.scatter(df["millis"], df["altp"], label="Altura Barométrica", color="black", s=5)

events = df[df["event"] != ""]
for _, row in events.iterrows():
    t = row["millis"]
    e = row["event"]
    plt.axvline(t, linestyle="--")
    plt.text(t, row["altp"], e, rotation=0, verticalalignment="bottom")

plt.xlabel("Tempo (s)")
plt.ylabel("Altitude (m)")
plt.title("Flight profile with detected events")
plt.grid(True)
plt.legend()
plt.show()
