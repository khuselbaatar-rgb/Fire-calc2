// ════════════════════════════════════════════════════════════════
//  Reinforced-Concrete Column Fire-Resistance — C++17 backend
//  3 бүлэг (группа) арматур:
//    As1 — булангийн стержнүүд (4 булан)
//          CORNER:      Θx · Θy  (хоёр граниас халдаг → хамгийн их)
//    As2 — доод/дээд граний стержнүүд
//          BOTTOM_FACE: Θy       (доод граниас a2 зайд)
//    As3 — зүүн/баруун граний стержнүүд
//          SIDE_FACE:   Θx       (хажуу граниас a3 зайд)
//  γ_s,tem бүлэг бүрт тусад нь интерполяцаар бодогдоно.
//  φ — λ-аас хамаарч шугаман интерполяцаар АВТОМАТААР.
// ════════════════════════════════════════════════════════════════
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <cmath>
#include <algorithm>

static double erf_approx(double x) {
    constexpr double a1 = 0.254829592, a2 = -0.284496736, a3 = 1.421413741;
    constexpr double a4 = -1.453152027, a5 = 1.061405429, p  = 0.3275911;
    int s = (x < 0) ? -1 : 1;
    x = std::abs(x);
    double t = 1.0 / (1.0 + p * x);
    double y = 1.0 - (((((a5*t + a4)*t) + a3)*t + a2)*t + a1) * t * std::exp(-x*x);
    return s * y;
}

static double interp(double x, const std::vector<std::pair<double,double>>& pts) {
    if (x <= pts.front().first) return pts.front().second;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        if (x <= pts[i].first) {
            double x0 = pts[i-1].first, y0 = pts[i-1].second;
            double x1 = pts[i].first,   y1 = pts[i].second;
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
        }
    }
    return pts.back().second;
}

// γ_s,tem — арматурын бат бэхийн бууралт халалтаас хамаарч (шугаман интерполяц)
static double gammaSteel(double t) {
    static const std::vector<std::pair<double,double>> pts = {
        {20, 1.0}, {300, 0.97}, {400, 0.85}, {500, 0.544}, {600, 0.37},
        {700, 0.22}, {800, 0.12}, {900, 0.06}, {1000, 0.03}
    };
    return interp(t, pts);
}

// φ — уян хатан байдлын коэффициент λ-аас хамаарч (шугаман интерполяц, АВТОМАТ)
static double phiByLambda(double lam) {
    static const std::vector<std::pair<double,double>> pts = {
        {8, 0.98}, {10, 0.973}, {11.5, 0.965}, {15, 0.95},
        {20, 0.90}, {30, 0.80}, {40, 0.70}, {50, 0.60}
    };
    return interp(lam, pts);
}

static std::map<std::string, double> readInput() {
    std::map<std::string, double> in;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        try { in[k] = std::stod(v); } catch (...) {}
    }
    return in;
}

static std::string num(double v, int prec = 10) {
    if (!std::isfinite(v)) return "null";
    std::ostringstream os;
    os << std::setprecision(prec) << v;
    return os.str();
}

static double get(const std::map<std::string,double>& m, const std::string& k, double def = 0.0) {
    auto it = m.find(k);
    return (it == m.end()) ? def : it->second;
}

// ── Халалтын чиглэл ──────────────────────────────────────────────
// Бүлэг бүрийн байршлаас хамаарч өөр чиглэлд халдаг:
//   CORNER      — As1: хоёр граниас (булан) → Θ = Θx · Θy
//   BOTTOM_FACE — As2: доод/дээд граниас   → Θ = Θy  (a2 = доод граниас зай)
//   SIDE_FACE   — As3: зүүн/баруун граниас → Θ = Θx  (a3 = хажуу граниас зай)

enum class HeatDir { CORNER, BOTTOM_FACE, SIDE_FACE };

struct RebarHeat { double ts; double g; };

// a   = тухайн граниас тэнхлэг хүртэлх зай (мм)
// dir = халалтын чиглэл
static RebarHeat rebarHeat(double a, double b, double h, double t0,
                            double root, double kbS, HeatDir dir) {
    RebarHeat rh{t0, 1.0};
    if (root <= 0.0) return rh;

    auto clamp01 = [](double v){ return std::max(0.0, std::min(1.0, v)); };

    double theta = 1.0;

    if (dir == HeatDir::CORNER) {
        // Булан: X ба Y хоёр чиглэлээс → Θ = Θx · Θy
        const double thetaX = clamp01(erf_approx((kbS + a)/root) + erf_approx((kbS + b - a)/root) - 1.0);
        const double thetaY = clamp01(erf_approx((kbS + a)/root) + erf_approx((kbS + h - a)/root) - 1.0);
        theta = thetaX * thetaY;

    } else if (dir == HeatDir::BOTTOM_FACE) {
        // Доод/дээд грань: зөвхөн Y чиглэл → Θ = Θy(a2)
        // a2 = доод граниас тэнхлэг хүртэл
        theta = clamp01(erf_approx((kbS + a)/root) + erf_approx((kbS + h - a)/root) - 1.0);

    } else { // SIDE_FACE
        // Зүүн/баруун грань: зөвхөн X чиглэл → Θ = Θx(a3)
        // a3 = хажуу граниас тэнхлэг хүртэл
        theta = clamp01(erf_approx((kbS + a)/root) + erf_approx((kbS + b - a)/root) - 1.0);
    }

    rh.ts = 1250.0 - (1250.0 - t0) * theta;
    rh.g  = gammaSteel(rh.ts);
    return rh;
}

struct StepResult {
    double tau, root, delta;
    double ts1, ts2, ts3;   // бүлэг бүрийн халалт
    double g1, g2, g3;      // бүлэг бүрийн γ
    double Nu;
    bool   ok;
};

static StepResult computeStep(double tau,
                               double b, double h,
                               double a1, double a2, double a3,
                               double Rbn, double Rsn,
                               double As1, double As2, double As3,
                               double t0, double Np, double phi,
                               double aRed, double kbS) {
    StepResult r{};
    r.tau   = tau;
    r.root  = (tau == 0.0) ? 0.0 : 2.0 * std::sqrt(std::max(aRed, 0.0) * tau * 60.0);
    r.ts1 = r.ts2 = r.ts3 = t0;
    r.g1  = r.g2  = r.g3  = 1.0;
    r.delta = 0.0;

    if (tau > 0.0) {
        // As1 — булангийн стержнүүд: CORNER (Θx·Θy, хоёр граниас)
        if (As1 > 0) {
            auto rh = rebarHeat(a1, b, h, t0, r.root, kbS, HeatDir::CORNER);
            r.ts1 = rh.ts; r.g1 = rh.g;
        }
        // As2 — доод/дээд граний стержнүүд: BOTTOM_FACE (Θy, доод граниас a2)
        if (As2 > 0) {
            auto rh = rebarHeat(a2, b, h, t0, r.root, kbS, HeatDir::BOTTOM_FACE);
            r.ts2 = rh.ts; r.g2 = rh.g;
        }
        // As3 — зүүн/баруун граний стержнүүд: SIDE_FACE (Θx, хажуу граниас a3)
        if (As3 > 0) {
            auto rh = rebarHeat(a3, b, h, t0, r.root, kbS, HeatDir::SIDE_FACE);
            r.ts3 = rh.ts; r.g3 = rh.g;
        }

        // Обугленный слой δ
        r.delta = std::max(0.0, std::min(std::min(b, h)/2.0 - 1.0,
                                          0.3807 * r.root - kbS));
    }

    const double bb = std::max(1.0, b - 2.0 * r.delta);
    const double hh = std::max(1.0, h - 2.0 * r.delta);

    // Σ γi·Rsn·Asi — зөвхөн байгаа бүлгийг л нэмнэ (Asi > 0 үед)
    double rebarSum = 0.0;
    if (As1 > 0) rebarSum += r.g1 * Rsn * As1;
    if (As2 > 0) rebarSum += r.g2 * Rsn * As2;
    if (As3 > 0) rebarSum += r.g3 * Rsn * As3;

    // Nu = φ · [Rbn·(b−2δ)(h−2δ) + Σ γi·Rsn·Asi] · 10⁻³
    r.Nu = phi * (Rbn*bb*hh + rebarSum) * 1.0e-3;
    r.ok = (r.Nu >= Np);
    return r;
}

int main() {
    auto in = readInput();

    const double b    = get(in, "b");
    const double h    = get(in, "h");
    const double H0   = get(in, "H0");
    const double kL   = get(in, "kL");
    const double a1   = get(in, "a1", get(in, "c1", 50.0));  // булангийн тэнхлэг зай
    const double a2   = get(in, "a2", get(in, "c1", 50.0));  // доод граниас зай (As2)
    const double a3   = get(in, "a3", get(in, "c1", 50.0));  // хажуу граниас зай (As3)
    const double Rbn  = get(in, "Rbn");
    const double Rsn  = get(in, "Rsn");
    const double rho  = get(in, "rho");
    const double W    = get(in, "W");
    const double tb   = get(in, "tb");
    const double t0   = get(in, "t0");
    const double As1  = get(in, "As1");
    const double As2  = get(in, "As2");
    const double As3  = get(in, "As3", 0.0);
    const double Np   = get(in, "Np");
    const double step = get(in, "step", 30.0);
    const double tmax = get(in, "tmax", 500.0);

    const bool   hasManualPhi = in.count("phiManual") > 0;
    const double phiManual    = hasManualPhi ? in["phiManual"] : 0.0;

    // ── Теплотехнические параметры ──
    constexpr double lambda0 = 1.14, aLambda = -0.00055;
    constexpr double c0      = 710.0, ac      = 0.84;
    const double lambdaTem = lambda0 + aLambda * tb;
    const double cTem      = c0      + ac      * tb;
    const double aRed_m2s  = lambdaTem / ((cTem + 50.0 * W) * rho);
    const double aRed      = aRed_m2s * 1.0e6;
    constexpr double kb    = 37.2;
    const double kbS       = kb * std::sqrt(std::max(aRed, 0.0));

    // ── Статик параметрүүд (τ=0) ──
    const double l0     = kL * H0;
    const double lambda = l0 / std::min(b, h);
    const double phi    = hasManualPhi ? phiManual : phiByLambda(lambda);
    const double AsTot  = As1 + As2 + As3;
    // N0 = φ · [Rbn·b·h + Σ Rsn·Asi] · 10⁻³  (τ=0, γi=1.0)
    double rebar0 = 0.0;
    if (As1 > 0) rebar0 += Rsn * As1;
    if (As2 > 0) rebar0 += Rsn * As2;
    if (As3 > 0) rebar0 += Rsn * As3;
    const double N0 = phi * (Rbn*b*h + rebar0) * 1.0e-3;

    // ── Хугацааны цэгүүд ──
    std::vector<double> times;
    for (double tau = 0; tau <= tmax + 1e-9; tau += step) times.push_back(tau);
    if (times.empty() || std::abs(times.back() - tmax) > 1e-9) times.push_back(tmax);

    std::ostringstream out;
    out << "{"
        << "\"lambdaTem\":" << num(lambdaTem) << ","
        << "\"cTem\":"      << num(cTem)      << ","
        << "\"aRed\":"      << num(aRed)      << ","
        << "\"kbS\":"       << num(kbS)       << ","
        << "\"l0\":"        << num(l0)        << ","
        << "\"lambda\":"    << num(lambda)    << ","
        << "\"phi\":"       << num(phi)       << ","
        << "\"phiManual\":" << (hasManualPhi ? "true" : "false") << ","
        << "\"As1\":"       << num(As1)       << ","
        << "\"As2\":"       << num(As2)       << ","
        << "\"As3\":"       << num(As3)       << ","
        << "\"AsTot\":"     << num(AsTot)     << ","
        << "\"N0\":"        << num(N0)        << ","
        << "\"Np\":"        << num(Np)        << ","
        << "\"N0pass\":"    << (N0 >= Np ? "true" : "false") << ","
        << "\"rows\":[";

    bool first = true;
    int  failIdx = -1;
    std::vector<double> Nus;
    Nus.reserve(times.size());

    for (std::size_t i = 0; i < times.size(); ++i) {
        const StepResult r = computeStep(times[i], b, h, a1, a2, a3,
                                          Rbn, Rsn, As1, As2, As3,
                                          t0, Np, phi, aRed, kbS);
        if (!r.ok && failIdx < 0) failIdx = static_cast<int>(i);
        Nus.push_back(r.Nu);

        if (!first) out << ",";
        first = false;
        out << "{"
            << "\"tau\":"   << num(r.tau)   << ","
            << "\"root\":"  << num(r.root)  << ","
            << "\"ts1\":"   << num(r.ts1)   << ","
            << "\"ts2\":"   << num(r.ts2)   << ","
            << "\"ts3\":"   << num(r.ts3)   << ","
            << "\"g1\":"    << num(r.g1)    << ","
            << "\"g2\":"    << num(r.g2)    << ","
            << "\"g3\":"    << num(r.g3)    << ","
            << "\"delta\":" << num(r.delta) << ","
            << "\"Nu\":"    << num(r.Nu)    << ","
            << "\"ok\":"    << (r.ok ? "true" : "false")
            << "}";
    }
    out << "],";

    // ── 1-минутын нягтралтай цуврал (график + Пф) ──
    out << "\"chartRows\":[";
    bool firstC = true;
    double prevTau = 0.0, prevNu = 0.0;
    bool firstPt = true, haveCross = false;
    double crossTau = -1.0;

    for (double tau = 0.0; tau <= tmax + 1e-9; tau += 1.0) {
        const StepResult r = computeStep(tau, b, h, a1, a2, a3,
                                          Rbn, Rsn, As1, As2, As3,
                                          t0, Np, phi, aRed, kbS);
        if (!firstPt && !haveCross && prevNu >= Np && r.Nu < Np) {
            crossTau = (r.Nu != prevNu)
                ? prevTau + (Np - prevNu) * (tau - prevTau) / (r.Nu - prevNu)
                : tau;
            haveCross = true;
        }
        prevTau = tau; prevNu = r.Nu; firstPt = false;

        if (!firstC) out << ",";
        firstC = false;
        out << "{"
            << "\"tau\":"   << num(r.tau)   << ","
            << "\"Nu\":"    << num(r.Nu)    << ","
            << "\"ts1\":"   << num(r.ts1)   << ","
            << "\"ts2\":"   << num(r.ts2)   << ","
            << "\"ts3\":"   << num(r.ts3)   << ","
            << "\"delta\":" << num(r.delta) << ","
            << "\"ok\":"    << (r.ok ? "true" : "false")
            << "}";
    }
    out << "],";

    // ── Дүгнэлт ──
    std::string verdict = "more";
    double tExact = -1.0;
    if (failIdx == 0) {
        verdict = "zero";
    } else if (haveCross) {
        tExact  = crossTau;
        verdict = "approx";
    } else if (failIdx > 0) {
        const double pT = times[failIdx - 1], pN = Nus[failIdx - 1];
        const double cT = times[failIdx],     cN = Nus[failIdx];
        if (cN != pN) tExact = pT + (Np - pN) * (cT - pT) / (cN - pN);
        verdict = "approx";
    }

    out << "\"verdict\":\"" << verdict     << "\","
        << "\"tExact\":"    << num(tExact) << ","
        << "\"tauLast\":"   << num(times.back())
        << "}";

    std::cout << out.str() << std::endl;
    return 0;
}



