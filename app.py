"""
Reinforced-Concrete Column Fire-Resistance Calculator
Python (Flask) frontend  +  C++ computational backend.

Run:
    python3 app.py
Then open http://localhost:5000
"""
from __future__ import annotations

import json
import os
import subprocess
from pathlib import Path

from flask import Flask, jsonify, render_template, request

BASE_DIR    = Path(__file__).resolve().parent
BACKEND_BIN = BASE_DIR / "backend" / "calculator.exe"


# Заавал шаардлагатай талбарууд (3 бүлэг арматурын шинэ бүтэц)
REQUIRED_KEYS = [
    "b", "h", "H0", "kL",
    "a1", "a2",
    "Rbn", "Rsn", "rho", "W", "tb", "t0",
    "As1", "As2", "Np", "step", "tmax",
]

# Заавал биш (байвал дамжуулна) — нэмэлт бүлэг ба хуучин түлхүүрүүд
OPTIONAL_KEYS = ["a3", "As3", "c1", "c2"]

app = Flask(__name__, template_folder="templates", static_folder="static")


@app.route("/")
def index():
    """Render the calculator UI."""
    return render_template("index.html")


@app.route("/api/calculate", methods=["POST"])
def calculate():
    """
    Forward the JSON payload to the C++ binary as `key=value` lines on stdin
    and return whatever JSON it prints on stdout.
    """
    if not BACKEND_BIN.exists():
        return jsonify({
            "error": (
                f"C++ backend not built. From the project root run:\n"
                f"    make -C backend"
            )
        }), 500

    payload = request.get_json(silent=True) or {}

    
    lines: list[str] = []
    for key in REQUIRED_KEYS:
        if key not in payload or payload[key] in ("", None):
            return jsonify({"error": f"Missing field: {key}"}), 400
        try:
            lines.append(f"{key}={float(payload[key])}")
        except (TypeError, ValueError):
            return jsonify({"error": f"Invalid number for {key}: {payload[key]!r}"}), 400

    # Заавал биш талбарууд — байвал дамжуулна (a3, As3 г.м.)
    for key in OPTIONAL_KEYS:
        val = payload.get(key)
        if val in ("", None):
            continue
        try:
            lines.append(f"{key}={float(val)}")
        except (TypeError, ValueError):
            return jsonify({"error": f"Invalid number for {key}: {val!r}"}), 400

    
    phi_manual = payload.get("phiManual")
    if phi_manual not in (None, ""):
        try:
            lines.append(f"phiManual={float(phi_manual)}")
        except (TypeError, ValueError):
            return jsonify({"error": f"Invalid phiManual: {phi_manual!r}"}), 400

    stdin_data = "\n".join(lines) + "\n"

    try:
        proc = subprocess.run(
            [str(BACKEND_BIN)],
            input=stdin_data,
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        )
    except subprocess.TimeoutExpired:
        return jsonify({"error": "Backend timed out"}), 504
    except subprocess.CalledProcessError as exc:
        return jsonify({"error": f"Backend failed: {exc.stderr.strip() or exc.returncode}"}), 500

    try:
        result = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return jsonify({"error": "Backend produced invalid JSON", "raw": proc.stdout}), 500

    
    def iso834_temp(tau_min):
        import math
        if tau_min <= 0: return 20
        return 345 * math.log10(8 * tau_min + 1) + 20

    fire_temps = []
    tauLast = int(result.get('tauLast', 180))
    for tau in range(0, tauLast + 1):
        fire_temps.append({'tau': tau, 'temp': round(iso834_temp(tau), 0)})
    result['fireTemps'] = fire_temps

    return jsonify(result)


@app.route("/api/health")
def health():
    return jsonify({
        "ok": True,
        "backend_built": BACKEND_BIN.exists(),
        "backend_path": str(BACKEND_BIN),
    })


if __name__ == "__main__":
    port = int(os.environ.get("PORT", "5000"))
    app.run(host="0.0.0.0", port=port, debug=True)
