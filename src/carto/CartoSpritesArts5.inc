// Sprites do Construir, leva 5 (porte do sprites5.js do Map Lab v37, aprovado em 16/11/2026):
//  · castelos de montanha (Ninho de Águia, Palácio em Terraços, Cidadela em Anéis), que assentam no
//    relevo como o Penhasco, e mais três (Forte Estrelado, Eslavo, Forte de Madeira);
//  · estátuas (Equestre, Anjo, Esfinge, Totens, Cabeça Caída);
//  · construções (Mosteiro, Palácio, Estalagem, Moinho d'Água, Observatório, Academia, Posto de Vigia, Santuário);
//  · as peças dos pincéis novos (tendas, iurtas, fogueira, ruínas, cais) e as plantações de pé (pomar, vinhedo, lavanda).
// Este arquivo é incluído DENTRO do namespace anônimo do CartoSpritesArts4.cpp: usa as peças da leva 4
// (ringL, lathe, frustum, curvedRoof, hipRoof, crystal, limb, disc, flame, lancet, roundWin, windowRows, figure...).

using Gz = std::function<double(double, double)>;
Gz gzOf(const Cam& cam) { return cam.gz ? cam.gz : Gz([](double, double) { return 0.0; }); }
Loc locAt(double x, double y, double yaw) { Art a; a.x = x; a.y = y; a.yaw = yaw; return Loc(a); }
V3 at3(const P2& p, double z) { return { p[0], p[1], z }; }
// vetor do sistema do artefato pro mundo, sem posição (pra eixos)
V3 dirL(const Art& b, double x, double y, double z = 0) { const double c = std::cos(b.yaw), s = std::sin(b.yaw); return { c * x - s * y, s * x + c * y, z }; }

void stoneCoursesDeco(QPainter& q, const PtFn& pt, double)
{
    strokeLines(q, QColor(60, 54, 48, 82), 0.6);
    for (int i = 1; i < 4; i++) line(q, pt(0, i / 4.0), pt(1, i / 4.0));
}
Deco smallWin(int n, double t0 = 0.35, double t1 = 0.68, RGB col = DARK)
{
    n = std::max(1, n);
    return [=](QPainter& q, const PtFn& pt, double kk) {
        for (int i = 0; i < n; i++) { const double s = (i + 0.5) / n; quad(q, pt, s - 0.24 / n, s + 0.24 / n, t0, t1, rgb(col, kk, 0.85)); }
    };
}
// viga com detalhe nas quatro faces de lado (listras do toucado, tábuas da rampa)
Faces beamD(const V3& p0, const V3& p1, double w, double h, const RGB& col, const std::function<Deco(int)>& deco)
{
    Faces f = beamFaces(p0, p1, w, h, col);
    for (int k = 0; k < 4 && k < static_cast<int>(f.size()); k++) f[k].deco = deco(k);
    return f;
}
// pé de um bloco no relevo: alicerce do ponto mais baixo do pé até o mais alto, e o bloco em cima
double onFoot(Faces& f, const Gz& gz, double cx, double cy, double yaw, double w, double d, const RGB& found, const std::function<Faces()>& build)
{
    const double c = std::cos(yaw), s = std::sin(yaw), hw = w / 2, hd = d / 2;
    const double pts[5][2] = { { -hw, -hd }, { hw, -hd }, { hw, hd }, { -hw, hd }, { 0, 0 } };
    double lo = 1e9, hi = -1e9;
    for (const auto& q : pts) { const double z = gz(cx + c * q[0] - s * q[1], cy + s * q[0] + c * q[1]); lo = std::min(lo, z); hi = std::max(hi, z); }
    if (hi - lo > 0.2) {
        const RGB top = shade(found, 0.92);
        append(f, lifted(boxFaces(cx, cy, yaw, w + 0.5, d + 0.5, 0, hi - lo + 0.7, found, &top, [](int, double) { return Deco(stoneCoursesDeco); }), lo - 0.7));
    }
    append(f, lifted(build(), hi));
    return hi;
}
// muralha que sobe e desce a encosta em pedaços curtos (como a do Penhasco)
void terrainWall(Faces& f, const P2& a, const P2& q, const Gz& gz, double wh, double th, const RGB& col, const RGB& top, bool merl)
{
    const double len = std::hypot(q[0] - a[0], q[1] - a[1]);
    if (len < 0.05) return;
    const int m = std::max(1, static_cast<int>(std::lround(len / 2.4)));
    const double dx = (q[0] - a[0]) / len, dy = (q[1] - a[1]) / len, nx = -dy * th / 2, ny = dx * th / 2, yaw = std::atan2(dy, dx);
    for (int i = 0; i < m; i++) {
        const P2 p0 = { a[0] + (q[0] - a[0]) * i / m, a[1] + (q[1] - a[1]) * i / m }, p1 = { a[0] + (q[0] - a[0]) * (i + 1) / m, a[1] + (q[1] - a[1]) * (i + 1) / m };
        const double z0 = gz(p0[0], p0[1]), z1 = gz(p1[0], p1[1]);
        const V3 C = { (p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, (z0 + z1) / 2 + wh / 2 };
        auto V = [&](const P2& p, double s, double z) { return V3{ p[0] + nx * s, p[1] + ny * s, z }; };
        f.push_back(face({ V(p0, 1, z0 - 2.5), V(p1, 1, z1 - 2.5), V(p1, 1, z1 + wh), V(p0, 1, z0 + wh) }, col, C, i % 2 ? slitDeco(1) : Deco()));
        f.push_back(face({ V(p1, -1, z1 - 2.5), V(p0, -1, z0 - 2.5), V(p0, -1, z0 + wh), V(p1, -1, z1 + wh) }, col, C, i % 2 ? Deco() : slitDeco(1)));
        f.push_back(face({ V(p0, 1, z0 + wh), V(p1, 1, z1 + wh), V(p1, -1, z1 + wh), V(p0, -1, z0 + wh) }, top, C));
        if (merl) {
            const double mx = (p0[0] + p1[0]) / 2, my = (p0[1] + p1[1]) / 2, mz = (z0 + z1) / 2 + wh;
            for (Face& x : boxFaces(mx, my, yaw, 0.6, th, mz, mz + 0.65, col)) { x.bias = 0.05; f.push_back(std::move(x)); }
        }
    }
}
// telhado de duas águas com a cumeeira no x local; oitões na cor da parede
Faces gableFaces(const Loc& L, double cx, double cy, double hw, double hd, double z, double rh, const RGB& wall, const RGB& roof, double ov = 0.18)
{
    const V3 C = L(cx, cy, z - 0.5);
    const V3 A = L(cx - hw - ov, cy - hd - ov, z - 0.05), B = L(cx + hw + ov, cy - hd - ov, z - 0.05), Cc = L(cx + hw + ov, cy + hd + ov, z - 0.05), D = L(cx - hw - ov, cy + hd + ov, z - 0.05);
    const V3 R1 = L(cx - hw - ov, cy, z + rh), R2 = L(cx + hw + ov, cy, z + rh);
    Faces f;
    f.push_back(face({ A, B, R2, R1 }, roof, C, roofDeco, 0.01));
    f.push_back(face({ Cc, D, R1, R2 }, roof, C, roofDeco, 0.01));
    Face t1 = face({ L(cx - hw, cy - hd, z), L(cx - hw, cy + hd, z), L(cx - hw, cy, z + rh - 0.1) }, wall, C); t1.q = { 0, 1, 2, 2 }; f.push_back(std::move(t1));
    Face t2 = face({ L(cx + hw, cy + hd, z), L(cx + hw, cy - hd, z), L(cx + hw, cy, z + rh - 0.1) }, wall, C); t2.q = { 0, 1, 2, 2 }; f.push_back(std::move(t2));
    return f;
}
// casinha de pedra/reboco com telhado de duas águas (cidadela)
void miniHouse(Faces& f, double x, double y, double yaw, double w, double d, double h, double z, const RGB& wall, const RGB& roof, double rh = -1)
{
    const Loc L = locAt(x, y, yaw);
    Faces fs = boxFaces(x, y, yaw, w, d, 0, h, wall, nullptr, [w](int k, double) { return k % 2 == 0 ? smallWin(static_cast<int>(std::lround(w / 1.3))) : Deco(); });
    fs.pop_back();
    append(fs, gableFaces(L, 0, 0, w / 2, d / 2, h, rh >= 0 ? rh : std::min(w, d) * 0.55, wall, roof));
    append(f, lifted(std::move(fs), z));
}
// cebola (cúpula russa) torneada em volta de (lx, ly) no sistema local
Faces onion(const Loc& L, double lx, double ly, double z, double r, const RGB& col)
{
    return lathe(L, lx, ly, { { r, z }, { r * 1.18, z + r * 0.42 }, { r * 1.08, z + r * 0.88 }, { r * 0.76, z + r * 1.22 }, { r * 0.36, z + r * 1.56 }, { r * 0.1, z + r * 1.84 }, { 0, z + r * 2.0 } }, 10, col);
}
// placa fina de duas faces (asa, placa da estalagem, pá da roda): cada lado com a sua normal
Faces plate(const std::vector<V3>& pts, const V3& nrm, double t, const RGB& col, std::optional<RGB> colB = std::nullopt)
{
    double l = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (!l) l = 1;
    const V3 N = { nrm[0] / l * t / 2, nrm[1] / l * t / 2, nrm[2] / l * t / 2 };
    V3 m = { 0, 0, 0 };
    for (const V3& p : pts) { m[0] += p[0]; m[1] += p[1]; m[2] += p[2]; }
    m = mul3(m, 1.0 / pts.size());
    std::vector<V3> A, B;
    for (const V3& p : pts) { A.push_back(add3(p, N)); B.push_back(sub3(p, N)); }
    std::reverse(B.begin(), B.end());
    Faces f;
    f.push_back(face(A, col, sub3(m, mul3(N, 4))));
    f.push_back(face(B, colB ? *colB : col, add3(m, mul3(N, 4))));
    return f;
}
// elipsoide com eixos quaisquer (cabeça caída, corpo do cavalo): centro c, semieixos ex, ey, ez (vetores)
Faces ellipsoid(const V3& c, const V3& ex, const V3& ey, const V3& ez, int n, int rings, const RGB& col, double seed, double jitter = 0.06, const std::vector<RGB>& cols = {})
{
    Rng R(seed);
    std::vector<std::vector<V3>> lat;
    for (int i = 0; i <= rings; i++) {
        const double phi = -M_PI / 2 + double(i) / rings * M_PI;
        std::vector<V3> row;
        for (int k = 0; k < n; k++) {
            const double th = double(k) / n * M_PI * 2, j = 1 + (R() - 0.5) * 2 * jitter;
            const double a = std::cos(phi) * std::cos(th) * j, b = std::cos(phi) * std::sin(th) * j, z = std::sin(phi) * j;
            row.push_back({ c[0] + ex[0] * a + ey[0] * b + ez[0] * z, c[1] + ex[1] * a + ey[1] * b + ez[1] * z, c[2] + ex[2] * a + ey[2] * b + ez[2] * z });
        }
        lat.push_back(std::move(row));
    }
    Faces f;
    for (int i = 0; i < rings; i++) for (int k = 0; k < n; k++) {
        const V3 &a = lat[i][k], &b = lat[i][(k + 1) % n], &cc = lat[i + 1][(k + 1) % n], &d = lat[i + 1][k];
        const RGB fc = cols.empty() ? col : cols[static_cast<size_t>(std::floor(R() * cols.size())) % cols.size()];
        if (i == 0) f.push_back(face({ a, cc, d }, fc, c));
        else if (i == rings - 1) f.push_back(face({ a, b, d }, fc, c));
        else f.push_back(face({ a, b, cc, d }, fc, c));
    }
    return f;
}
// o que fica abaixo do chão é achatado nele (cabeça meio enterrada, entulho)
Faces clampBelow(Faces faces, double z0 = 0)
{
    Faces out;
    for (Face& q : faces) {
        if (std::none_of(q.v.begin(), q.v.end(), [&](const V3& p) { return p[2] > z0 + 0.05; })) continue;
        for (V3& p : q.v) p[2] = std::max(z0, p[2]);
        out.push_back(std::move(q));
    }
    return out;
}
P2 edgeOut(const P2& a, const P2& b)
{
    const double dx = b[0] - a[0], dy = b[1] - a[1];
    double l = std::hypot(dx, dy);
    if (!l) l = 1;
    return { dy / l, -dx / l };
}
// contorno com recuo em quina viva (d > 0 empurra pra fora)
std::vector<P2> offsetPoly(const std::vector<P2>& pts, double d)
{
    const size_t n = pts.size();
    std::vector<P2> out;
    for (size_t i = 0; i < n; i++) {
        const P2& p = pts[i];
        const P2 n1 = edgeOut(pts[(i + n - 1) % n], p), n2 = edgeOut(p, pts[(i + 1) % n]);
        const double dot = n1[0] * n2[0] + n1[1] * n2[1], k = d / std::max(0.25, 1 + dot);
        out.push_back({ p[0] + (n1[0] + n2[0]) * k, p[1] + (n1[1] + n2[1]) * k });
    }
    return out;
}
int pickOf(double v, int mul, int n) { return static_cast<int>(std::floor(v * mul)) % n; }

// ════════════════════════ castelos ════════════════════════

// Ninho de Águia: o castelo de conto de fadas no alto do morro. Palácio branco comprido de telhado íngreme,
// torre de menagem quadrada, torrinhas finas de cone altíssimo, a guarita de tijolo na frente. Assenta no
// relevo como o Penhasco: cada peça no seu chão, com alicerce descendo.
CastleOut4 eyrieCastle(const Art& b, const Cam& cam)
{
    const CastleCfg& cfg = cfgOf(b);
    const int sz = clampi(b.size, 0, 3);
    const double R = cfg.R;
    const Loc L(b);
    Rng Rr(seedOf(b, 51));
    const Gz gz = gzOf(cam);
    static const RGB whites[3] = { { 240, 234, 220 }, { 234, 228, 214 }, { 244, 240, 230 } }, slates[3] = { { 80, 98, 128 }, { 72, 88, 112 }, { 92, 108, 136 } };
    const RGB white = whites[pickOf(b.v, 3, 3)], slate = slates[pickOf(b.v, 7, 3)];
    const RGB brick = { 178, 104, 80 }, found = { 170, 160, 142 }, gold = { 214, 178, 96 };
    CastleOut4 o;
    Faces& f = o.faces;
    // muralha baixa num oval torto, seguindo o chão
    const int n = 8 + sz * 2;
    std::vector<P2> pts;
    for (int k = 0; k < n; k++) {
        const double a = double(k) / n * M_PI * 2 + 0.2;
        const double rx = std::cos(a) * R * (0.95 + Rr() * 0.1);
        const double ry = std::sin(a) * R * 0.62 * (0.9 + Rr() * 0.18);
        pts.push_back(L.at(rx, ry));
    }
    for (int k = 0; k < n; k++) terrainWall(f, pts[k], pts[(k + 1) % n], gz, cfg.wallH * 0.62, 0.9, white, shade(white, 0.9), true);
    auto turret = [&](double x, double y, double r, double h, const RGB& col) {
        const double z = gz(x, y);
        append(f, lifted(taperFaces(x, y, r * 1.18, r * 1.04, -2.6, 0, 8, found), z));
        append(f, lifted(taperFaces(x, y, r, r, 0, h, 8, col, [](int j) { return j % 2 ? smallWin(1, 0.55, 0.75) : Deco(); }), z));
        append(f, lifted(coneFaces(x, y, r * 1.15, h, r * 4.2, 8, slate), z));
        append(f, lifted(beamFaces({ x, y, h + r * 4.2 - 0.2 }, { x, y, h + r * 4.2 + 0.7 }, 0.1, 0.1, gold), z));
        return z + h + r * 4.2;
    };
    for (int k = 0; k < n; k += 2) turret(pts[k][0], pts[k][1], cfg.towerR * 0.45, cfg.wallH * 1.15, white);
    // o palácio comprido no fundo, telhado alto de ardósia
    const double kw = cfg.keep * 1.45, kd = cfg.keep * 0.55, kh = cfg.keepH * 0.92, pcx = -R * 0.3, pcy = 0;
    const P2 pp = L.at(pcx, pcy);
    onFoot(f, gz, pp[0], pp[1], b.yaw, kw, kd, found, [&]() {
        Faces fs = boxFaces(pp[0], pp[1], b.yaw, kw, kd, 0, kh, white, nullptr, [](int k, double) { return windowRows(3, k % 2 ? 2 : 6, { 62, 72, 92 }); });
        fs.pop_back();
        append(fs, gableFaces(L, pcx, pcy, kw / 2, kd / 2, kh, kd * 1.15, white, slate));
        return fs;
    });
    // torrinhas finas nas pontas do palácio
    const double corner[4][3] = { { kw / 2, kd / 2, 1.25 }, { kw / 2, -kd / 2, 1.4 }, { -kw / 2, kd / 2, 1.15 }, { -kw / 2, -kd / 2, 1.3 } };
    const int nc = 2 + std::min(2, sz + 1);
    for (int i = 0; i < nc; i++) { const P2 q = L.at(pcx + corner[i][0], pcy + corner[i][1]); turret(q[0], q[1], cfg.towerR * 0.55, kh * corner[i][2], white); }
    // menagem quadrada na frente do palácio, a mais alta, telhado de pirâmide
    const double tw = cfg.towerR * 1.55, th = cfg.keepH * 1.55, tcx = R * 0.18, tcy = -R * 0.2;
    const P2 tp = L.at(tcx, tcy);
    const double tz = onFoot(f, gz, tp[0], tp[1], b.yaw, tw, tw, found, [&]() {
        Faces fs = boxFaces(tp[0], tp[1], b.yaw, tw, tw, 0, th, white, nullptr, [](int, double) -> Deco {
            return [](QPainter& q, const PtFn& pt, double kk) {
                slitDeco(1)(q, pt, kk);
                const PtFn sub = [&pt](double s, double t) { return pt(s, 0.78 + t * 0.2); };
                windowRows(1, 2, { 62, 72, 92 })(q, sub, kk);
            };
        });
        fs.pop_back();
        append(fs, frustum(L, tcx, tcy, tw / 2 + 0.25, tw / 2 + 0.25, 0, 0, th, th + tw * 1.7, slate));
        return fs;
    });
    // guarita de tijolo na frente com duas torrinhas
    const double gcx = R * 0.92;
    const P2 gp = L.at(gcx, 0);
    onFoot(f, gz, gp[0], gp[1], b.yaw, 2.6, 3.4, found, [&]() {
        const RGB bt = shade(brick, 0.9);
        return boxFaces(gp[0], gp[1], b.yaw, 2.6, 3.4, 0, cfg.wallH * 1.25, brick, &bt, [](int k, double) { return (k == 0 || k == 2) ? Deco(gateDeco) : Deco(); });
    });
    for (int s : { -1, 1 }) { const P2 q = L.at(gcx, s * 1.9); turret(q[0], q[1], 0.75, cfg.wallH * 1.55, brick); }
    o.flag = { tp[0], tp[1], tz + th + tw * 1.7 };
    return o;
}

// Palácio em Terraços: sobe a encosta em degraus de muralha inclinada (branca embaixo, ocre lá em cima), janelas
// em fileira, a faixa escura no topo de cada degrau e os pavilhões dourados no alto. A frente olha morro abaixo
// sozinha; no plano, os degraus recuam pra trás.
CastleOut4 terracedCastle(const Art& b, const Cam& cam)
{
    const CastleCfg& cfg = cfgOf(b);
    const int sz = clampi(b.size, 0, 3);
    const double R = cfg.R;
    const Gz gz = gzOf(cam);
    static const RGB ochres[3] = { { 156, 58, 44 }, { 142, 52, 46 }, { 166, 72, 46 } };
    const RGB white = { 244, 240, 230 }, ochre = ochres[pickOf(b.v, 3, 3)], gold = { 222, 180, 80 }, band = { 118, 40, 34 };
    const double dd = R * 0.6, gxv = gz(b.x + dd, b.y) - gz(b.x - dd, b.y), gyv = gz(b.x, b.y + dd) - gz(b.x, b.y - dd), gl = std::hypot(gxv, gyv);
    const double ux = gl > 0.8 ? gxv / gl : -std::cos(b.yaw), uy = gl > 0.8 ? gyv / gl : -std::sin(b.yaw), yaw = std::atan2(-uy, -ux);
    static const int tiersBy[4] = { 2, 3, 4, 5 };
    const int tiers = tiersBy[sz];
    const double hx0 = R * 0.9, hy0 = R * 1.0;
    CastleOut4 o;
    Faces& f = o.faces;
    double prevTop = 0, lhx = 0, lhy = 0, ltz = 0;
    Loc lastL = locAt(b.x, b.y, yaw);
    for (int i = 0; i < tiers; i++) {
        const bool top = i == tiers - 1;
        const double sh = 0.62 / std::max(1, tiers - 1), hx = hx0 * (1 - i * sh), hy = hy0 * (1 - i * sh * (top ? 1.15 : 0.7)), off = (hx0 - hx) * 0.92;
        const Loc L = locAt(b.x + ux * off, b.y + uy * off, yaw);
        double lo = 1e9, hi = -1e9;
        for (const P2& q : { P2{ hx, hy }, P2{ hx, -hy }, P2{ -hx, hy }, P2{ -hx, -hy }, P2{ 0, 0 } }) {
            const P2 w = L.at(q[0], q[1]);
            const double z = gz(w[0], w[1]);
            lo = std::min(lo, z); hi = std::max(hi, z);
        }
        const double tierH = (cfg.wallH * 0.7 + R * 0.22) * (top ? 1.2 : 1);
        const double base = i == 0 ? lo - 1.2 : prevTop - 0.4, tz = std::max(i == 0 ? hi : prevTop, hi) + tierH;
        const RGB col = top ? ochre : white, topc = top ? shade(ochre, 0.85) : RGB{ 212, 204, 186 };
        const int rows = std::max(1, static_cast<int>(std::lround((tz - base) / 2.2)));
        append(f, frustum(L, 0, 0, hx, hy, hx - 0.45, hy - 0.45, base, tz, col, &topc, [=](int k) -> Deco {
            return [=](QPainter& q, const PtFn& pt, double kk) {
                const double len = k % 2 ? hx * 2 : hy * 2, t1 = 0.84;
                const int n = std::max(2, static_cast<int>(std::lround(len / 1.7)));
                for (int r = 0; r < rows; r++) for (int j = 0; j < n; j++) {
                    const double s = (j + 0.5) / n, t0 = 0.12 + (r + 0.25) / rows * (t1 - 0.12);
                    quad(q, pt, s - 0.18 / n, s + 0.18 / n, t0, t0 + 0.3 / rows * (t1 - 0.12), rgb(DARK, kk, 0.8));
                }
                quad(q, pt, 0, 1, t1, 1, rgb(top ? RGB{ 96, 30, 26 } : band, kk, 0.9));
                if (i == 0 && k == 0) quad(q, pt, 0.44, 0.56, 0, std::min(0.3, 3 / std::max(3.0, tz - base)), rgb({ 70, 40, 30 }, kk));   // o portão
            };
        }));
        prevTop = tz; lastL = L; lhx = hx; lhy = hy; ltz = tz;
    }
    // pavilhões dourados no topo
    const int np = 1 + std::min(2, sz);
    for (int j = 0; j < np; j++) {
        const double ly = np == 1 ? 0 : (double(j) / (np - 1) - 0.5) * lhy * 1.1, s = j == np / 2 ? 1.25 : 0.9, w = std::min(lhx, lhy) * 0.32 * s;
        const P2 q = lastL.at(-lhx * 0.2, ly);
        append(f, boxFaces(q[0], q[1], yaw, w * 2, w * 2, ltz, ltz + w * 1.1, ochre, nullptr, [](int, double) { return smallWin(1, 0.3, 0.7); }));
        append(f, curvedRoof(locAt(q[0], q[1], yaw), 0, 0, w + 0.6, w * 0.22, ltz + w * 1.1, 0.35, w * 1.1, gold));
    }
    const P2 fp = lastL.at(-lhx * 0.2, 0);
    o.flag = { fp[0], fp[1], ltz + std::min(lhx, lhy) * 0.8 + 1.4 };
    return o;
}

// Cidadela em Anéis: muralhas concêntricas, cada anel um degrau acima do outro, com casas nos terraços e a torre
// branca de agulha prateada no alto. No morro, os anéis acompanham a subida.
CastleOut4 tieredCastle(const Art& b, const Cam& cam)
{
    const CastleCfg& cfg = cfgOf(b);
    const int sz = clampi(b.size, 0, 3);
    const double R = cfg.R * 1.05;
    const Loc L(b);
    Rng Rr(seedOf(b, 61));
    const Gz gz = gzOf(cam);
    static const RGB stones[3] = { { 228, 224, 212 }, { 220, 216, 206 }, { 232, 226, 210 } };
    static const RGB roofs[4] = { { 122, 112, 108 }, { 150, 84, 64 }, { 98, 108, 122 }, { 138, 98, 74 } };
    const RGB stone = stones[pickOf(b.v, 3, 3)], top = shade(stone, 0.9), pave = { 204, 198, 184 }, silver = { 206, 212, 222 };
    static const int ringsBy[4] = { 2, 3, 4, 5 };
    const int rings = ringsBy[sz];
    const double rise = cfg.wallH * 0.85;
    CastleOut4 o;
    Faces& f = o.faces;
    auto ringPts = [&](double r, int m) {
        std::vector<P2> v;
        for (int k = 0; k < m; k++) v.push_back(L.at(std::cos(double(k) / m * M_PI * 2) * r, std::sin(double(k) / m * M_PI * 2) * r));
        return v;
    };
    double prev = 1e9;
    for (const P2& q : ringPts(R, 12)) prev = std::min(prev, gz(q[0], q[1]));
    prev -= 0.3;
    double rIn = R;
    for (int i = 0; i < rings; i++) {
        const double r = R * (1 - i / (rings + 0.6)), rn = R * (1 - (i + 1) / (rings + 0.6));
        const int m = std::max(12, static_cast<int>(std::lround(M_PI * 2 * r / 2.4)));
        const std::vector<P2> P = ringPts(r, m);
        double gmax = -1e9;
        for (const P2& q : ringPts((r + rn) / 2, 12)) gmax = std::max(gmax, gz(q[0], q[1]));
        const double lvl = std::max(prev + rise, gmax + 0.8);
        const int gateK = static_cast<int>(std::lround(((i % 2 ? 0.5 : 0) + (Rr() - 0.5) * 0.1) * m + m)) % m;
        for (int k = 0; k < m; k++) {
            const P2 a = P[k], q = P[(k + 1) % m];
            const double th = 1.0, len = std::hypot(q[0] - a[0], q[1] - a[1]), dx = (q[0] - a[0]) / len, dy = (q[1] - a[1]) / len;
            const double nx = -dy * th / 2, ny = dx * th / 2, zt = lvl + 1.1;
            const double z0a = i == 0 ? gz(a[0], a[1]) - 1.8 : prev - 0.3, z0b = i == 0 ? gz(q[0], q[1]) - 1.8 : prev - 0.3;
            auto V = [&](const P2& p, double s, double z) { return V3{ p[0] + nx * s, p[1] + ny * s, z }; };
            const V3 C = { (a[0] + q[0]) / 2, (a[1] + q[1]) / 2, zt - 2 };
            f.push_back(face({ V(a, 1, z0a), V(q, 1, z0b), V(q, 1, zt), V(a, 1, zt) }, stone, C, k == gateK ? Deco(gateDeco) : k % 3 == 0 ? slitDeco(1) : Deco()));
            f.push_back(face({ V(q, -1, z0b), V(a, -1, z0a), V(a, -1, zt), V(q, -1, zt) }, stone, C));
            f.push_back(face({ V(a, 1, zt), V(q, 1, zt), V(q, -1, zt), V(a, -1, zt) }, top, C));
            if (k % 2 == 0)
                for (Face& x : boxFaces((a[0] + q[0]) / 2, (a[1] + q[1]) / 2, std::atan2(dy, dx), 0.6, th, zt, zt + 0.6, stone)) { x.bias = 0.05; f.push_back(std::move(x)); }
        }
        // piso do terraço (o anel entre esta muralha e a de dentro)
        const std::vector<P2> Pn = ringPts(rn, m);
        for (int k = 0; k < m; k++)
            f.push_back(face({ at3(P[k], lvl), at3(P[(k + 1) % m], lvl), at3(Pn[(k + 1) % m], lvl), at3(Pn[k], lvl) }, pave, { b.x, b.y, lvl - 3 }, Deco(), -0.02));
        // casas no terraço, viradas pra rua
        const double mid = (r + rn) / 2, width = r - rn;
        if (width > 2.6) {
            const int nh = static_cast<int>(std::floor(M_PI * 2 * mid / 3.1));
            for (int j = 0; j < nh; j++) {
                if (Rr() < 0.18) continue;
                const double a = (j + 0.5) / nh * M_PI * 2 + b.yaw, rr = mid + (Rr() - 0.5) * width * 0.25;
                const double x = b.x + std::cos(a) * rr, y = b.y + std::sin(a) * rr;
                const double wd = std::min(2.2, width * 0.55), wl = 2.1 + Rr() * 0.6;
                const double hh = 1.4 + Rr() * 0.9;
                const RGB wc = shade(stone, 0.96 + Rr() * 0.06);
                const RGB rc = roofs[static_cast<int>(std::floor(Rr() * 4)) % 4];
                miniHouse(f, x, y, a + M_PI / 2, wl, wd, hh, lvl, wc, rc);
            }
        }
        prev = lvl; rIn = rn;
    }
    // a torre branca no alto, com o salão encostado
    const double kr = cfg.kt[0] * 1.35, kt = cfg.kt[1] * 1.05;
    append(f, taperFaces(b.x, b.y, kr * 1.15, kr, prev, prev + kt, 12, stone, [](int j) { return j % 3 == 0 ? smallWin(1, 0.7, 0.85) : Deco(); }));
    append(f, ringL(L, 0, 0, kr * 1.3, kr * 1.3, kr * 1.3, kr * 1.3, prev + kt, prev + kt + 0.5, 12, top));
    append(f, coneFaces(b.x, b.y, kr * 1.05, prev + kt + 0.5, cfg.kt[2] * 1.7, 12, silver));
    const double hw = std::min(rIn * 0.55, cfg.keep * 0.45);
    if (hw > 1.2) {
        const P2 hp = L.at(-kr - hw * 0.8, 0);
        const Loc Lh = locAt(hp[0], hp[1], b.yaw);
        Faces fs = boxFaces(hp[0], hp[1], b.yaw, hw * 1.6, hw * 1.1, prev, prev + cfg.keepH * 0.35, stone, nullptr, [](int k, double) { return k % 2 ? Deco() : windowRows(1, 3, DARK); });
        fs.pop_back();
        append(f, std::move(fs));
        append(f, gableFaces(Lh, 0, 0, hw * 0.8, hw * 0.55, prev + cfg.keepH * 0.35, hw * 0.7, stone, roofs[2]));
    }
    o.flag = { b.x, b.y, prev + kt + 0.5 + cfg.kt[2] * 1.7 };
    return o;
}

// Forte Estrelado: muralha baixa e inclinada em estrela (baluartes), topo gramado, canhões nas pontas, fosso seco
// em volta, quartéis e o comando no pátio de areia
CastleOut4 starCastle(const Art& b)
{
    const CastleCfg& cfg = cfgOf(b);
    const int sz = clampi(b.size, 0, 3);
    const double R = cfg.R;
    static const RGB stones[3] = { { 188, 172, 146 }, { 178, 164, 140 }, { 196, 180, 150 } };
    const RGB stone = stones[pickOf(b.v, 3, 3)], grass = { 118, 142, 84 }, sand = { 206, 192, 156 };
    CastleOut4 o;
    Faces& f = o.faces;
    static const int nPBy[4] = { 4, 5, 5, 6 };
    const int nP = nPBy[sz];
    const double Ro = R * 1.15, Ri = R * 0.72;
    std::vector<P2> poly;
    for (int k = 0; k < nP * 2; k++) { const double a = double(k) / (nP * 2) * M_PI * 2 + b.yaw, r = k % 2 ? Ri : Ro; poly.push_back({ b.x + std::cos(a) * r, b.y + std::sin(a) * r }); }
    const double zt = cfg.wallH * 0.85, ob = std::min(2.2, R * 0.2), ib = std::min(2.4, R * 0.17);
    const std::vector<P2> oB = offsetPoly(poly, ob), iT = offsetPoly(poly, -ib), ditch = offsetPoly(poly, ob + std::min(3.4, R * 0.3));
    const size_t N = poly.size();
    // fosso seco (chão mais escuro) e o pátio
    for (size_t k = 0; k < N; k++) {
        const size_t k1 = (k + 1) % N;
        o.ground.push_back(flatPoly({ at3(oB[k], 0.05), at3(oB[k1], 0.05), at3(ditch[k1], 0.05), at3(ditch[k], 0.05) }, { 98, 116, 76 }, { b.x, b.y, -1 }));
    }
    { std::vector<V3> v; for (const P2& p : iT) v.push_back(at3(p, 0.06)); o.ground.push_back(flatPoly(v, sand, { b.x, b.y, -1 })); }
    for (size_t k = 0; k < N; k++) {
        const size_t k1 = (k + 1) % N;
        const P2 n = edgeOut(poly[k], poly[k1]), m = { (poly[k][0] + poly[k1][0]) / 2, (poly[k][1] + poly[k1][1]) / 2 };
        const V3 cOut = { m[0] - n[0] * 3, m[1] - n[1] * 3, zt / 2 }, cIn = { m[0] + n[0] * 6, m[1] + n[1] * 6, zt / 2 };
        f.push_back(face({ at3(oB[k], -0.4), at3(oB[k1], -0.4), at3(poly[k1], zt), at3(poly[k], zt) }, stone, cOut, [](QPainter& q, const PtFn& pt, double) {
            strokeLines(q, QColor(70, 60, 48, 77), 0.6);
            for (int i = 1; i < 4; i++) line(q, pt(0, i / 4.0), pt(1, i / 4.0));
        }));
        f.push_back(face({ at3(poly[k], zt), at3(poly[k1], zt), at3(iT[k1], zt), at3(iT[k], zt) }, grass, { m[0], m[1], zt - 3 }));
        f.push_back(face({ at3(iT[k], zt), at3(iT[k1], zt), at3(iT[k1], 0), at3(iT[k], 0) }, shade(stone, 0.95), cIn));
        // parapeito de pedra na borda de fora
        const P2 pa = { poly[k][0] - n[0] * 0.3, poly[k][1] - n[1] * 0.3 }, pb = { poly[k1][0] - n[0] * 0.3, poly[k1][1] - n[1] * 0.3 };
        for (Face& x : boxFaces((pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2, std::atan2(pb[1] - pa[1], pb[0] - pa[0]), std::hypot(pb[0] - pa[0], pb[1] - pa[1]), 0.6, zt, zt + 0.55, stone)) {
            x.bias = 0.03;
            f.push_back(std::move(x));
        }
    }
    // canhões nas pontas
    for (size_t k = 0; k < N; k += 2) {
        const P2 p = poly[k];
        const double dx = p[0] - b.x, dy = p[1] - b.y, l = std::hypot(dx, dy), ux = dx / l, uy = dy / l;
        for (int s : { -1, 1 }) {
            const double bx = p[0] - ux * 2.2 - uy * s * 0.9, by = p[1] - uy * 2.2 + ux * s * 0.9;
            append(f, beamFaces({ bx, by, zt + 0.45 }, { bx + ux * 1.6, by + uy * 1.6, zt + 0.6 }, 0.36, 0.36, { 52, 52, 56 }));
        }
    }
    // quartéis em volta do pátio e o comando no meio
    const int nb = nP >= 5 ? 3 : 2;
    const RGB plaster = { 226, 214, 192 }, tile = { 168, 92, 64 };
    for (int j = 0; j < nb; j++) {
        const double a = (j + 0.5) / nb * M_PI * 2 + b.yaw + M_PI / nP, d = Ri * 0.5, x = b.x + std::cos(a) * d, y = b.y + std::sin(a) * d, yaw = a + M_PI / 2, len = Ri * 0.62;
        Faces fs = boxFaces(x, y, yaw, len, 1.9, 0, 1.9, plaster, nullptr, [len](int k, double) { return k % 2 ? Deco() : smallWin(static_cast<int>(std::lround(len / 1.4))); });
        fs.pop_back();
        append(f, std::move(fs));
        append(f, hipRoof(locAt(x, y, yaw), 0, 0, len / 2 + 0.2, 1.15, 1.9, 1.0, tile, roofDeco));
    }
    const double cw = 2.4 + sz * 0.4;
    append(f, boxFaces(b.x, b.y, b.yaw, cw, cw, 0, 3.0, plaster, nullptr, [](int, double) { return smallWin(2); }));
    append(f, hipRoof(Loc(b), 0, 0, cw / 2 + 0.2, cw / 2 + 0.2, 3.0, 1.2, tile, roofDeco));
    o.flag = { b.x, b.y, 4.3 };
    return o;
}

// Eslavo: muralha de tijolo vermelho com ameias rabo-de-andorinha, torres de telhado de tenda verde, a catedral
// branca de cinco cebolas coloridas e o campanário branco de cúpula dourada
void swallowMerlons(Faces& f, const P2& a, const P2& q, double z, double thick, const RGB& col)
{
    const double len = std::hypot(q[0] - a[0], q[1] - a[1]), yaw = std::atan2(q[1] - a[1], q[0] - a[0]), c = std::cos(yaw), s = std::sin(yaw);
    const int n = std::max(2, static_cast<int>(std::floor(len / 1.3)));
    for (int k = 0; k < n; k++) {
        const double t = (k + 0.5) / n, x = a[0] + (q[0] - a[0]) * t, y = a[1] + (q[1] - a[1]) * t;
        for (Face& fc : boxFaces(x, y, yaw, 0.7, thick, z, z + 0.55, col)) { fc.bias = 0.05; f.push_back(std::move(fc)); }
        for (int sd : { -1, 1 })
            for (Face& fc : boxFaces(x + c * sd * 0.24, y + s * sd * 0.24, yaw, 0.22, thick, z + 0.55, z + 0.95, col)) { fc.bias = 0.06; f.push_back(std::move(fc)); }
    }
}
CastleOut4 slavicCastle(const Art& b)
{
    const CastleCfg& cfg = cfgOf(b);
    const int sz = clampi(b.size, 0, 3);
    const double R = cfg.R;
    const Loc L(b);
    Rng Rr(seedOf(b, 71));
    static const RGB bricks[3] = { { 176, 72, 56 }, { 164, 66, 52 }, { 186, 84, 62 } }, tents[3] = { { 62, 118, 92 }, { 54, 104, 82 }, { 124, 52, 46 } };
    static const RGB domes[4] = { { 62, 104, 170 }, { 70, 140, 110 }, { 196, 70, 64 }, { 228, 186, 80 } };
    const RGB brick = bricks[pickOf(b.v, 3, 3)], top = shade(brick, 0.86), white = { 242, 238, 228 }, tent = tents[pickOf(b.v, 5, 3)], gold = { 228, 186, 80 };
    const RGB winC = { 70, 64, 60 };
    CastleOut4 o;
    Faces& f = o.faces;
    static const int nBy[4] = { 4, 5, 5, 6 };
    const int n = nBy[sz];
    std::vector<P2> pts;
    for (int k = 0; k < n; k++) {
        const double a = double(k) / n * M_PI * 2 + M_PI / n;
        const double rx = std::cos(a) * R * (0.92 + Rr() * 0.16);
        const double ry = std::sin(a) * R * (0.92 + Rr() * 0.16);
        pts.push_back({ rx, ry });
    }
    const double wh = cfg.wallH * 1.05;
    for (int k = 0; k < n; k++) {
        const P2 a = L.at(pts[k][0], pts[k][1]), q = L.at(pts[(k + 1) % n][0], pts[(k + 1) % n][1]);
        const double mx = (a[0] + q[0]) / 2, my = (a[1] + q[1]) / 2, len = std::hypot(q[0] - a[0], q[1] - a[1]), yaw = std::atan2(q[1] - a[1], q[0] - a[0]);
        for (Face& x : boxFaces(mx, my, yaw, len, 1.3, 0, wh, brick, &top, [len](int s, double) { return s % 2 == 0 ? slitDeco(std::max(2, static_cast<int>(std::lround(len / 2.6)))) : Deco(); })) {
            x.c = { mx, my, wh / 2 };
            f.push_back(std::move(x));
        }
        swallowMerlons(f, a, q, wh, 1.3, brick);
    }
    auto tentTower = [&](double x, double y, double w, double h, bool tall) {
        append(f, boxFaces(x, y, b.yaw, w, w, 0, h, brick, &top, [white](int, double) -> Deco {
            return [white](QPainter& q, const PtFn& pt, double kk) { slitDeco(2)(q, pt, kk); quad(q, pt, 0, 1, 0.9, 0.96, rgb(white, kk, 0.9)); };
        }));
        double z = h;
        if (tall) {
            append(f, boxFaces(x, y, b.yaw, w * 0.72, w * 0.72, z, z + w * 0.9, brick, &top, [white, gold](int, double) -> Deco {
                return [white, gold](QPainter& q, const PtFn& pt, double kk) {      // o relógio
                    const QPointF c = pt(0.5, 0.55), e = pt(0.75, 0.55);
                    const double r = std::max(1.0, std::fabs(e.x() - c.x()));
                    q.setPen(pen(rgb(gold, kk), 0.8)); q.setBrush(rgb(white, kk)); q.drawEllipse(c, r, r);
                };
            }));
            z += w * 0.9;
        }
        append(f, taperFaces(x, y, w * 0.4, w * 0.38, z, z + w * 0.55, 8, brick, [](int j) { return j % 2 ? slitDeco(1) : Deco(); }));
        z += w * 0.55;
        append(f, coneFaces(x, y, w * 0.47, z, w * (tall ? 2.6 : 1.9), 8, tent));
        z += w * (tall ? 2.6 : 1.9);
        append(f, beamFaces({ x, y, z - 0.2 }, { x, y, z + 0.8 }, 0.12, 0.12, gold));
        return z + 0.8;
    };
    for (const P2& p : pts) { const P2 w = L.at(p[0], p[1]); tentTower(w[0], w[1], cfg.towerR * 1.6, cfg.towerH * 0.9, false); }
    // a torre do portão no meio da muralha da frente, a mais alta (com o relógio)
    int fk = 0;
    for (int i = 0; i < n; i++) if (pts[(i + 1) % n][0] + pts[i][0] > pts[(fk + 1) % n][0] + pts[fk][0]) fk = i;
    const P2 gpl = { (pts[fk][0] + pts[(fk + 1) % n][0]) / 2, (pts[fk][1] + pts[(fk + 1) % n][1]) / 2 }, gw = L.at(gpl[0], gpl[1]);
    const double gTop = tentTower(gw[0], gw[1], cfg.towerR * 2.0, cfg.towerH * 1.05, true);
    // catedral: corpo branco, cinco tambores com cebolas
    const double kw = cfg.keep * 1.0, kd = cfg.keep * 0.82, kh = cfg.keepH * 0.5, ccx = -R * 0.12, ccy = R * 0.08;
    const P2 cp = L.at(ccx, ccy);
    const RGB wTop = shade(white, 0.88);
    append(f, boxFaces(cp[0], cp[1], b.yaw, kw, kd, 0, kh, white, &wTop, [winC](int k, double) { return roundWin(k % 2 ? 2 : 3, winC); }));
    auto drum = [&](double dx, double dy, double r, double h, const RGB& col) {
        const P2 w = L.at(ccx + dx, ccy + dy);
        append(f, taperFaces(w[0], w[1], r, r * 0.96, kh, kh + h, 10, white, [winC](int j) { return j % 2 ? roundWin(1, winC, 0.25, 0.75) : Deco(); }));
        append(f, onion(L, ccx + dx, ccy + dy, kh + h, r * 1.02, col));
        append(f, beamFaces({ w[0], w[1], kh + h + r * 2.0 - 0.1 }, { w[0], w[1], kh + h + r * 2.0 + 0.9 }, 0.08, 0.08, gold));
        append(f, beamFaces({ w[0], w[1] - 0.3, kh + h + r * 2.0 + 0.6 }, { w[0], w[1] + 0.3, kh + h + r * 2.0 + 0.6 }, 0.08, 0.08, gold));
    };
    const double dr = std::min(kw, kd) * 0.2;
    for (const P2& sxy : { P2{ 1, 1 }, P2{ 1, -1 }, P2{ -1, 1 }, P2{ -1, -1 } }) {
        const RGB dc = domes[static_cast<int>(std::floor(Rr() * 3)) % 3];
        drum(sxy[0] * kw * 0.28, sxy[1] * kd * 0.28, dr * 0.75, dr * 1.4, dc);
    }
    drum(0, 0, dr, dr * 2.0, gold);
    // campanário em andares octogonais
    const double blx = ccx - kw * 0.85, bly = ccy + kd * 0.75;
    const P2 bp = L.at(blx, bly);
    double z = 0, br = cfg.towerR * 0.95;
    for (int i = 0; i < 3; i++) {
        const double h = cfg.keepH * (i ? 0.28 : 0.4);
        append(f, taperFaces(bp[0], bp[1], br, br * 0.92, z, z + h, 8, white, [winC](int j) { return j % 2 ? roundWin(1, winC, 0.3, 0.8) : Deco(); }));
        append(f, taperFaces(bp[0], bp[1], br * 1.06, br * 1.06, z + h - 0.25, z + h, 8, shade(white, 0.9)));
        z += h; br *= 0.8;
    }
    append(f, lathe(L, blx, bly, { { br * 1.1, z }, { br * 1.05, z + br * 0.6 }, { br * 0.6, z + br * 1.05 }, { 0, z + br * 1.25 } }, 10, gold));
    if (sz >= 2) {      // o palácio amarelo encostado na muralha
        const double pw = cfg.keep * 1.4, pd = cfg.keep * 0.42;
        const P2 pp = L.at(-R * 0.55, -R * 0.42);
        Faces fs = boxFaces(pp[0], pp[1], b.yaw, pd * 2, pw, 0, cfg.keepH * 0.38, { 234, 212, 150 }, nullptr, [winC](int k, double) { return windowRows(2, k % 2 ? 6 : 2, winC); });
        fs.pop_back();
        append(f, std::move(fs));
        append(f, hipRoof(locAt(pp[0], pp[1], b.yaw), 0, 0, pd + 0.2, pw / 2 + 0.2, cfg.keepH * 0.38, pd * 0.9, tent, roofDeco));
    }
    o.flag = { gw[0], gw[1], gTop };
    return o;
}

// Forte de Madeira: o morro de terra (mota) com a torre de toras e a paliçada lá em cima, a paliçada grande em volta
// do pátio, o salão comprido de palha, as cabanas e a rampa subindo pro morro
Faces palFaces(const P2& a, const P2& q, double z0, double h, double k = 1)
{
    static const RGB LOGc = { 128, 94, 62 };
    const double len = std::hypot(q[0] - a[0], q[1] - a[1]) + 0.2, yaw = std::atan2(q[1] - a[1], q[0] - a[0]);
    const double c = std::cos(yaw), s = std::sin(yaw), ox = -s * 0.35 * k, oy = c * 0.35 * k;
    const int n = std::max(2, static_cast<int>(std::lround(len / (0.55 * k))));
    Faces faces;
    for (int side : { -1, 1 }) {
        std::vector<V3> v = { { a[0] + ox * side, a[1] + oy * side, z0 - 1.0 * k }, { q[0] + c * 0.1 + ox * side, q[1] + s * 0.1 + oy * side, z0 - 1.0 * k } };
        for (int i = n; i >= 0; i--) {
            const double t = double(i) / n;
            v.push_back({ a[0] + (q[0] + c * 0.1 - a[0]) * t + ox * side, a[1] + (q[1] + s * 0.1 - a[1]) * t + oy * side, z0 + (i % 2 ? h : h * 0.8) });
        }
        const int last = static_cast<int>(v.size()) - 1;
        Face fc = face(v, side > 0 ? LOGc : shade(LOGc, 0.94), { (a[0] + q[0]) / 2, (a[1] + q[1]) / 2, z0 + 1 }, [n](QPainter& qq, const PtFn& pt, double) {
            strokeLines(qq, QColor(52, 34, 20, 140), 0.6);
            for (int i = 1; i < n; i++) line(qq, pt(double(i) / n, 0), pt(double(i) / n, 0.86));
        });
        fc.q = { 0, 1, 2, last };
        faces.push_back(std::move(fc));
    }
    return faces;
}
const Deco kPlanks = [](QPainter& q, const PtFn& pt, double) {
    strokeLines(q, QColor(52, 34, 20, 115), 0.6);
    for (int i = 1; i < 6; i++) line(q, pt(0, i / 6.0), pt(1, i / 6.0));
};
CastleOut4 motteCastle(const Art& b)
{
    const CastleCfg& cfg = cfgOf(b);
    const int sz = clampi(b.size, 0, 3);
    const double R = cfg.R;
    const Loc L(b);
    Rng Rr(seedOf(b, 81));
    static const RGB grasses[3] = { { 116, 140, 80 }, { 108, 132, 74 }, { 124, 146, 84 } };
    const RGB wood = { 132, 98, 64 }, grass = grasses[pickOf(b.v, 3, 3)], thatch = { 180, 152, 98 }, shingle = { 112, 86, 62 }, LOGc = { 128, 94, 62 };
    CastleOut4 o;
    Faces& f = o.faces;
    const double Rx = R, Ry = R * 0.76;
    // paliçada grande do pátio (com a abertura do portão na frente)
    const int m = std::max(14, static_cast<int>(std::lround(M_PI * (Rx + Ry) / 2.2)));
    std::vector<P2> pp;
    for (int k = 0; k <= m; k++) { const double a = double(k) / m * M_PI * 2; pp.push_back(L.at(std::cos(a) * Rx, std::sin(a) * Ry)); }
    const double gapA = 1.9 / Rx;
    for (int k = 0; k < m; k++) {
        const double a0 = double(k) / m * M_PI * 2, a1 = double(k + 1) / m * M_PI * 2;
        if ((a0 < gapA || a0 > M_PI * 2 - gapA) || (a1 < gapA || a1 > M_PI * 2 - gapA)) continue;
        append(f, palFaces(pp[k], pp[k + 1], 0, 3.0));
    }
    // torre do portão
    const P2 gp = L.at(Rx, 0);
    const double gw = 1.6;
    for (const P2& d : { P2{ -1, -1 }, P2{ 1, -1 }, P2{ 1, 1 }, P2{ -1, 1 } }) { const P2 q = L.at(Rx + d[0] * 0.9, d[1] * gw); append(f, beamFaces({ q[0], q[1], -0.5 }, { q[0], q[1], 5.2 }, 0.32, 0.32, LOGc)); }
    const RGB wTop = shade(wood, 0.85);
    append(f, boxFaces(gp[0], gp[1], b.yaw, 2.6, gw * 2 + 0.8, 4.4, 5.4, wood, &wTop));
    append(f, frustum(L, Rx, 0, 1.6, gw + 0.7, 0, gw * 0.3, 5.4, 6.8, shingle));
    // a mota: morro de terra gramado, com a paliçada e a torre de toras em cima
    const double mx = -R * 0.4, Rm = R * 0.4 + 1.3, hm = cfg.wallH * 1.2 + 1.2;
    append(f, lathe(L, mx, 0, { { Rm, 0 }, { Rm * 0.86, hm * 0.45 }, { Rm * 0.62, hm * 0.88 }, { Rm * 0.52, hm }, { 0, hm } }, 16, grass));
    const double rt = Rm * 0.47;
    const int mt = 10;
    std::vector<P2> tp;
    for (int k = 0; k <= mt; k++) { const double a = double(k) / mt * M_PI * 2 + 0.3; tp.push_back(L.at(mx + std::cos(a) * rt, std::sin(a) * rt)); }
    for (int k = 0; k < mt; k++) append(f, palFaces(tp[k], tp[k + 1], hm, 1.9, 0.8));
    const double kw = Rm * 0.52, kh = cfg.keepH * 0.42;
    const P2 kp = L.at(mx, 0);
    append(f, boxFaces(kp[0], kp[1], b.yaw, kw, kw, hm, hm + kh, wood, nullptr, [](int, double) -> Deco {
        return [](QPainter& q, const PtFn& pt, double kk) { kPlanks(q, pt, kk); slitDeco(1)(q, pt, kk); };
    }));
    append(f, boxFaces(kp[0], kp[1], b.yaw, kw + 0.8, kw + 0.8, hm + kh, hm + kh + 1.3, shade(wood, 1.06), nullptr, [](int, double) { return smallWin(3, 0.3, 0.7); }));
    append(f, frustum(L, mx, 0, kw / 2 + 0.6, kw / 2 + 0.6, 0, 0, hm + kh + 1.3, hm + kh + 1.3 + kw * 0.9, shingle));
    // rampa de tábuas do pátio até o alto do morro
    const P2 r0 = L.at(mx + Rm * 1.05, -1.2), r1 = L.at(mx + rt * 0.9, -1.2);
    append(f, beamD({ r0[0], r0[1], 0.6 }, { r1[0], r1[1], hm + 0.2 }, 1.0, 0.18, { 150, 116, 78 }, [](int) { return kPlanks; }));
    // salão comprido de palha e cabanas redondas
    const double hl = 4.2 + sz * 1.2;
    const P2 hp = L.at(R * 0.32, R * 0.38);
    Faces hfs = boxFaces(hp[0], hp[1], b.yaw, hl, 2.4, 0, 1.6, wood, nullptr, [](int k, double) { return k % 2 == 0 ? kPlanks : Deco(); });
    hfs.pop_back();
    append(f, std::move(hfs));
    append(f, gableFaces(locAt(hp[0], hp[1], b.yaw), 0, 0, hl / 2, 1.2, 1.6, 1.8, wood, thatch, 0.35));
    const int nh = 2 + sz;
    for (int j = 0; j < nh; j++) {
        const double a = -0.6 - j * 0.55 + (Rr() - 0.5) * 0.2;
        const P2 q = L.at(std::cos(a) * Rx * 0.62 + R * 0.1, std::sin(a) * Ry * 0.62);
        append(f, taperFaces(q[0], q[1], 1.0, 0.95, 0, 1.1, 8, wood));
        append(f, coneFaces(q[0], q[1], 1.25, 1.05, 1.5, 8, thatch));
    }
    o.flag = { kp[0], kp[1], hm + kh + 1.3 + kw * 0.9 };
    return o;
}

// os castelos da leva 5 (false = não é deles)
bool castle5(const Art& b, const Cam& cam, CastleOut4& o)
{
    const QString& s = b.style;
    if (s == QLatin1String("eyrie")) o = eyrieCastle(b, cam);
    else if (s == QLatin1String("terraced")) o = terracedCastle(b, cam);
    else if (s == QLatin1String("tiered")) o = tieredCastle(b, cam);
    else if (s == QLatin1String("star")) o = starCastle(b);
    else if (s == QLatin1String("slavic")) o = slavicCastle(b);
    else if (s == QLatin1String("motte")) o = motteCastle(b);
    else return false;
    return true;
}

// ════════════════════════ estátuas ════════════════════════
const RGB kBronze = { 96, 136, 116 }, kMarble = { 232, 228, 220 }, kSand5 = { 214, 184, 130 };

// Equestre: o cavaleiro de bronze com a espada erguida, o cavalo com uma pata dianteira levantada
void drawEquestrian(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB marble = { 214, 208, 196 }, col = kBronze;
    Faces f = boxFaces(b.x, b.y, b.yaw, 5.0, 2.4, 0, 0.5, shade(marble, 0.94));
    append(f, boxFaces(b.x, b.y, b.yaw, 4.4, 1.9, 0.5, 2.3, marble, nullptr, [](int k, double) -> Deco {
        if (k % 2) return Deco();
        return [](QPainter& q, const PtFn& pt, double kk) {
            QPainterPath pa; pa.moveTo(pt(0.3, 0.35)); pa.lineTo(pt(0.7, 0.35)); pa.lineTo(pt(0.7, 0.65)); pa.lineTo(pt(0.3, 0.65)); pa.closeSubpath();
            q.setBrush(Qt::NoBrush); q.setPen(pen(rgb(DARK, kk, 0.4), 0.7)); q.drawPath(pa);
        };
    }));
    append(f, boxFaces(b.x, b.y, b.yaw, 4.8, 2.2, 2.3, 2.6, shade(marble, 1.03)));
    const double z0 = 2.6;
    auto P = [&](double x, double y, double z) { return L(x, y, z0 + z); };
    append(f, ellipsoid(P(0, 0, 2.15), dirL(b, 1.45, 0), dirL(b, 0, 0.62), { 0, 0, 0.7 }, 10, 6, col, seedOf(b, 5), 0.03));
    // pernas: traseiras firmes, uma dianteira levantada
    for (int s : { -1, 1 }) {
        append(f, limb(P(-0.95, s * 0.35, 1.9), P(-1.05, s * 0.35, 0.95), 0.26, 0.17, 6, col));
        append(f, limb(P(-1.05, s * 0.35, 0.95), P(-0.95, s * 0.35, 0.05), 0.15, 0.13, 6, col));
    }
    append(f, limb(P(0.95, 0.35, 1.9), P(1.0, 0.35, 0.95), 0.25, 0.16, 6, col)); append(f, limb(P(1.0, 0.35, 0.95), P(1.05, 0.35, 0.05), 0.15, 0.13, 6, col));
    append(f, limb(P(0.95, -0.35, 1.9), P(1.45, -0.35, 1.35), 0.25, 0.16, 6, col)); append(f, limb(P(1.45, -0.35, 1.35), P(1.35, -0.35, 0.8), 0.15, 0.13, 6, col));
    // pescoço, cabeça, crina e rabo
    append(f, limb(P(1.05, 0, 2.45), P(1.75, 0, 3.55), 0.42, 0.28, 8, col)); append(f, limb(P(1.75, 0, 3.65), P(2.4, 0, 3.15), 0.27, 0.15, 8, col));
    append(f, beamFaces(P(1.0, 0, 2.95), P(1.6, 0, 3.85), 0.12, 0.3, shade(col, 0.85)));
    append(f, limb(P(-1.4, 0, 2.4), P(-1.95, 0, 1.25), 0.16, 0.07, 6, shade(col, 0.9)));
    // cavaleiro
    append(f, limb(P(-0.05, 0, 2.85), P(0.05, 0, 4.1), 0.36, 0.3, 8, col));
    append(f, ellipsoid(P(0.08, 0, 4.48), { 0.26, 0, 0 }, { 0, 0.24, 0 }, { 0, 0, 0.3 }, 8, 5, col, seedOf(b, 7), 0.02));
    for (int s : { -1, 1 }) append(f, limb(P(0, s * 0.3, 2.95), P(0.35, s * 0.62, 2.3), 0.16, 0.12, 6, col));
    append(f, limb(P(0.02, -0.32, 3.9), P(0.35, -0.45, 3.35), 0.12, 0.1, 6, col)); append(f, limb(P(0.35, -0.45, 3.35), P(0.85, -0.3, 3.05), 0.1, 0.09, 6, col));
    append(f, limb(P(0.02, 0.34, 3.95), P(0.35, 0.55, 4.6), 0.12, 0.1, 6, col)); append(f, limb(P(0.35, 0.55, 4.6), P(0.55, 0.62, 5.3), 0.1, 0.09, 6, col));
    append(f, beamFaces(P(0.58, 0.63, 5.25), P(0.95, 0.68, 7.0), 0.16, 0.06, { 168, 190, 176 }));
    append(f, beamFaces(P(0.5, 0.42, 5.3), P(0.62, 0.86, 5.36), 0.1, 0.1, col));
    append(f, plate({ P(-0.2, -0.32, 4.0), P(-0.2, 0.32, 4.0), P(-0.7, 0.42, 2.7), P(-0.75, -0.42, 2.7) }, dirL(b, 1, 0), 0.08, shade(col, 0.9)));
    drawFaces(p, f, cam);
}

// Anjo: figura de túnica, asas abertas atrás dos ombros, a espada apoiada à frente; mármore branco
void drawAngel(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    Faces f = lathe(L, 0, 0, { { 1.7, 0 }, { 1.7, 0.5 }, { 1.35, 0.6 }, { 1.15, 0.7 }, { 1.15, 2.2 }, { 1.4, 2.35 }, { 1.4, 2.7 }, { 0, 2.7 } }, 10, { 202, 196, 186 });
    const double z0 = 2.7, H = 7.2;
    FigOpt o;
    o.robe = true; o.robeCol = shade(kMarble, 0.97); o.yaw = b.yaw;
    o.armL = Arm{ { 0.1, -0.1, 0.62 }, { 0.15, -0.02, 0.55 } };
    o.armR = Arm{ { 0.1, 0.1, 0.62 }, { 0.15, 0.02, 0.56 } };
    append(f, figure(L, z0, H, kMarble, o));
    auto P = [&](double x, double y, double t) { return L(x * H, y * H, z0 + t * H); };
    append(f, beamFaces(P(0.16, 0, 0.6), P(0.18, 0, 0.0), 0.05 * H, 0.015 * H, { 214, 214, 220 }));
    append(f, beamFaces(P(0.16, -0.08, 0.53), P(0.16, 0.08, 0.53), 0.03 * H, 0.03 * H, kMarble));
    // asas: cada uma um leque de penas grandes (placas finas) que varrem pra trás conforme abrem
    static const double tips[7][3] = { { -0.16, 0.2, 0.48 }, { -0.2, 0.36, 0.58 }, { -0.22, 0.5, 0.75 }, { -0.22, 0.58, 0.95 }, { -0.18, 0.52, 1.12 }, { -0.12, 0.36, 1.2 }, { -0.07, 0.2, 1.06 } };
    for (int s : { -1, 1 }) {
        const V3 sh = P(-0.04, s * 0.1, 0.78);
        for (int i = 0; i < 6; i++) {
            const V3 A = P(tips[i][0] - tips[i][1] * 0.45, s * tips[i][1], tips[i][2]), B = P(tips[i + 1][0] - tips[i + 1][1] * 0.45, s * tips[i + 1][1], tips[i + 1][2]);
            append(f, plate({ sh, A, B }, dirL(b, 1, 0, 0.15), 0.06, i % 2 ? kMarble : shade(kMarble, 0.96), shade(kMarble, 0.9)));
        }
    }
    drawFaces(p, f, cam);
}

// Esfinge: corpo de leão deitado, patas estendidas, cabeça com o toucado listrado; arenito na base
void drawSphinx(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB sand = kSand5, dark = shade(kSand5, 0.82), base = { 196, 172, 128 }, baseTop = shade(base, 0.92);
    Faces f = boxFaces(b.x, b.y, b.yaw, 10.2, 5.0, 0, 1.0, base, &baseTop, [](int k, double) { return k % 2 == 0 ? Deco(stoneCoursesDeco) : Deco(); });
    auto P = [&](double x, double y, double z) { return L(x, y, z + 1.0); };
    append(f, ellipsoid(P(-1.0, 0, 1.25), dirL(b, 3.1, 0), dirL(b, 0, 1.45), { 0, 0, 1.3 }, 12, 6, sand, seedOf(b, 3), 0.04));
    for (int s : { -1, 1 }) append(f, ellipsoid(P(-2.9, s * 1.05, 0.95), dirL(b, 1.1, 0), dirL(b, 0, 0.6), { 0, 0, 0.95 }, 8, 5, sand, seedOf(b, 4 + s), 0.04));
    for (int s : { -1, 1 }) {
        append(f, beamFaces(P(1.2, s * 0.85, 0.35), P(4.3, s * 0.85, 0.32), 0.75, 0.65, sand));
        const P2 q = L.at(4.4, s * 0.85);
        append(f, boxFaces(q[0], q[1], b.yaw, 0.6, 0.85, 1.0, 1.65, sand));
    }
    // peito e cabeça
    append(f, ellipsoid(P(1.5, 0, 1.6), dirL(b, 0.9, 0), dirL(b, 0, 1.1), { 0, 0, 1.3 }, 10, 6, sand, seedOf(b, 6), 0.03));
    {
        const P2 q = L.at(1.9, 0);
        append(f, boxFaces(q[0], q[1], b.yaw, 1.3, 1.25, 3.0, 4.5, sand, &sand, [](int k, double) -> Deco {
            if (k) return Deco();
            return [](QPainter& qq, const PtFn& pt, double kk) {
                quad(qq, pt, 0.22, 0.38, 0.58, 0.66, rgb(DARK, kk, 0.6)); quad(qq, pt, 0.62, 0.78, 0.58, 0.66, rgb(DARK, kk, 0.6));
                strokeLines(qq, rgb(DARK, kk, 0.5), 0.7);
                line(qq, pt(0.5, 0.55), pt(0.5, 0.35)); line(qq, pt(0.38, 0.22), pt(0.62, 0.22));
            };
        }));
    }
    // toucado: as abas listradas caindo dos dois lados e a tampa
    const Deco stripe = [](QPainter& q, const PtFn& pt, double kk) {
        strokeLines(q, rgb({ 70, 96, 140 }, kk, 0.55), 1);
        for (int i = 1; i < 6; i++) line(q, pt(0, i / 6.0), pt(1, i / 6.0));
    };
    for (int s : { -1, 1 }) append(f, beamD(P(1.8, s * 0.82, 3.55), P(1.7, s * 1.05, 2.05), 0.95, 0.22, sand, [stripe](int) { return stripe; }));
    append(f, frustum(L, 1.75, 0, 0.8, 0.85, 0.55, 0.62, 4.5, 4.95, sand, nullptr, [stripe](int) { return stripe; }));
    append(f, beamFaces(P(1.45, 0, 3.0), P(1.35, 0, 1.9), 0.4, 0.3, dark));
    // rabo encostado no flanco
    append(f, limb(P(-3.7, 1.2, 0.3), P(-1.6, 1.5, 0.25), 0.16, 0.14, 6, dark)); append(f, limb(P(-1.6, 1.5, 0.25), P(-1.0, 1.35, 0.6), 0.14, 0.2, 6, dark));
    drawFaces(p, f, cam);
}

// Totens: três postes entalhados em blocos coloridos com rostos, o mais alto com o pássaro de asas abertas
void drawTotems(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB cols[5] = { { 176, 64, 48 }, { 44, 40, 38 }, { 62, 132, 128 }, { 226, 210, 172 }, { 210, 160, 70 } };
    static const double poles[3][4] = { { 0, 0, 9.5, 0.62 }, { -1.2, -1.9, 7.2, 0.52 }, { -0.9, 2.0, 6.2, 0.5 } };
    const std::function<Deco(int, int)> faceDeco = [](int j, int n) -> Deco {
        if (j != 0 && j != n - 1) return Deco();
        return [j](QPainter& q, const PtFn& pt, double kk) {
            const QPointF a = pt(j == 0 ? 0.3 : 0.7, 0.62), e = pt(j == 0 ? 0.45 : 0.85, 0.62);
            const double d = std::fabs(e.x() - a.x()), r1 = std::max(0.8, d), r2 = std::max(0.4, d * 0.45);
            q.setPen(Qt::NoPen);
            q.setBrush(rgb({ 250, 244, 230 }, kk, 0.95)); q.drawEllipse(a, r1, r1);
            q.setBrush(rgb(DARK, kk)); q.drawEllipse(a, r2, r2);
            quad(q, pt, j == 0 ? 0.2 : 0.4, j == 0 ? 0.6 : 0.8, 0.2, 0.3, rgb(DARK, kk));
        };
    };
    Faces f;
    for (int pi = 0; pi < 3; pi++) {
        const double lx = poles[pi][0], ly = poles[pi][1], h = poles[pi][2], r = poles[pi][3];
        const P2 xy = L.at(lx, ly);
        const Loc Lp = locAt(xy[0], xy[1], b.yaw);
        const int seg = std::max(3, static_cast<int>(std::lround(h / 1.6)));
        const double sh = h / seg;
        for (int i = 0; i < seg; i++) {
            const RGB c0 = cols[(i + pi) % 5];
            append(f, ringL(Lp, 0, 0, r, r * 0.9, r, r * 0.9, i * sh, (i + 1) * sh - 0.06, 8, c0, faceDeco));
            append(f, ringL(Lp, 0, 0, r * 1.08, r * 1.0, r * 1.08, r * 1.0, (i + 1) * sh - 0.12, (i + 1) * sh, 8, shade(c0, 0.8)));
            const P2 np = Lp.at(r * 0.9, 0);
            append(f, beamFaces({ np[0], np[1], i * sh + sh * 0.45 }, { np[0] + std::cos(b.yaw) * 0.45, np[1] + std::sin(b.yaw) * 0.45, i * sh + sh * 0.35 }, 0.22, 0.18, shade(c0, 0.9)));
        }
        if (pi == 0) {
            for (int s : { -1, 1 })
                append(f, plate({ { xy[0], xy[1], h - 0.4 }, L(lx - 0.2, ly + s * 2.6, h + 0.6), L(lx - 0.3, ly + s * 2.4, h - 0.2), L(lx - 0.1, ly + s * 1.1, h - 1.0) }, dirL(b, 0.2, 0, 1), 0.12, cols[0], shade(cols[0], 0.8)));
            append(f, crystal(L(lx + 0.4, ly, h - 0.2), dirL(b, 1, 0, -0.2), 0.3, 0.22, 4, cols[4], 3));
        }
    }
    drawFaces(p, f, cam);
}

// Cabeça Caída: a cabeça de uma estátua gigante tombada de lado, meio enterrada, com a coroa quebrada e pedaços
// em volta; pedra gasta com musgo
void drawFallenHead(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB stone = { 172, 166, 154 }, moss = { 124, 138, 96 };
    const std::vector<RGB> cols = { stone, shade(stone, 0.95), shade(stone, 1.04), stone, shade(stone, 0.98), mix(stone, moss, 0.6) };
    const V3 U = norm(dirL(b, 0.25, -1, 0.1)), F = norm(dirL(b, 0.45, 0.1, 1.0)), Sd = norm(cross(U, F));
    const V3 C = L(0, 0, 1.6);
    auto A = [&](double u, double s, double fw) { return V3{ C[0] + U[0] * u + Sd[0] * s + F[0] * fw, C[1] + U[1] * u + Sd[1] * s + F[1] * fw, C[2] + U[2] * u + Sd[2] * s + F[2] * fw }; };
    Faces f = clampBelow(ellipsoid(C, mul3(U, 3.3), mul3(Sd, 2.5), mul3(F, 2.7), 12, 8, stone, seedOf(b, 9), 0.05, cols));
    // nariz, sobrancelha, lábios
    append(f, clampBelow(beamFaces(A(0.1, 0, 2.3), A(-0.9, 0, 3.15), 0.7, 0.55, stone)));
    append(f, clampBelow(beamFaces(A(0.75, -1.2, 2.25), A(0.75, 1.2, 2.25), 0.5, 0.45, shade(stone, 0.95))));
    append(f, clampBelow(beamFaces(A(-1.55, -0.6, 2.35), A(-1.55, 0.6, 2.35), 0.3, 0.35, shade(stone, 0.9))));
    // olhos: órbitas fundas e escuras
    for (int sd : { -1, 1 }) {
        Faces e = ellipsoid(A(0.3, sd * 0.95, 2.3), mul3(U, 0.36), mul3(Sd, 0.55), mul3(F, 0.22), 8, 4, { 70, 66, 60 }, seedOf(b, 20 + sd), 0.02);
        for (Face& q : e) q.bias = 0.02;
        append(f, std::move(e));
    }
    // coroa: pontas quebradas saindo pela "nuca de cima"
    for (int i = 0; i < 5; i++) {
        const double a = (i / 4.0 - 0.5) * 2.2;
        const V3 pp = A(2.6 + std::cos(a) * 0.3, std::sin(a) * 1.9, std::cos(a) * 1.2 - 0.4);
        const V3 dir = { U[0] + Sd[0] * std::sin(a) * 0.4, U[1] + Sd[1] * std::sin(a) * 0.4, U[2] + Sd[2] * std::sin(a) * 0.4 };
        append(f, clampBelow(crystal(pp, dir, 1.0 + (i % 2) * 0.9 - (i == 3 ? 0.7 : 0), 0.42, 4, { 206, 172, 92 }, i == 3 ? 0.3 : 1.8)));
    }
    // pedaços: um tambor de coluna e blocos
    append(f, limb(L(-3.6, 3.0, 0.55), L(-1.6, 3.6, 0.55), 0.62, 0.62, 8, stone));
    { const P2 q = L.at(3.6, -2.4); append(f, clampBelow(blobFaces(q[0], q[1], 0.3, 0.9, 0.6, 5, 2, shade(stone, 0.95), seedOf(b, 12), 0.25))); }
    { const P2 q = L.at(-3.0, -3.1); append(f, clampBelow(blobFaces(q[0], q[1], 0.2, 0.7, 0.5, 5, 2, moss, seedOf(b, 13), 0.25))); }
    drawFaces(p, f, cam);
}

// ════════════════════════ construções ════════════════════════

// Mosteiro: casas brancas de faixa vermelha e telhado dourado agarradas na encosta, cada uma no seu chão, ligadas
// por escadas; bandeirolas de oração esticadas entre elas. No plano, um pátio de casas.
void drawMonastery(QPainter& p, const Art& b, const Cam& cam)
{
    const Gz gz = gzOf(cam);
    const Loc L(b);
    const RGB white = { 244, 240, 232 }, band = { 134, 44, 38 }, gold = { 220, 176, 78 }, found = { 176, 166, 150 };
    struct Bl { double lx, ly, w, d, h; bool main; };
    static const Bl bl[5] = { { 0, 0, 3.4, 2.8, 3.2, true }, { -2.6, 2.4, 2.4, 2.0, 2.0, false }, { 2.2, 2.6, 2.0, 1.8, 1.8, false },
                              { -2.4, -2.6, 2.6, 2.0, 2.4, false }, { 2.6, -2.0, 1.8, 1.6, 1.6, false } };
    std::vector<V3> tops;
    Faces f;
    for (const Bl& q : bl) {
        const P2 xy = L.at(q.lx, q.ly);
        const double z = onFoot(f, gz, xy[0], xy[1], b.yaw, q.w, q.d, found, [&]() {
            Faces fs = boxFaces(xy[0], xy[1], b.yaw, q.w, q.d, 0, q.h, white, nullptr, [&q, band](int k, double) -> Deco {
                const int n = std::max(1, static_cast<int>(std::lround((k % 2 ? q.d : q.w) / 1.1)));
                return [n, band](QPainter& qq, const PtFn& pt, double kk) {
                    for (int i = 0; i < n; i++) {
                        const double s = (i + 0.5) / n;
                        QPainterPath pa; pa.moveTo(pt(s - 0.28 / n, 0.35)); pa.lineTo(pt(s + 0.28 / n, 0.35)); pa.lineTo(pt(s + 0.2 / n, 0.66)); pa.lineTo(pt(s - 0.2 / n, 0.66)); pa.closeSubpath();
                        qq.fillPath(pa, rgb(DARK, kk, 0.85));
                    }
                    quad(qq, pt, 0, 1, 0.8, 1, rgb(band, kk));
                };
            });
            fs.pop_back();
            if (q.main) append(fs, curvedRoof(locAt(xy[0], xy[1], b.yaw), 0, 0, std::max(q.w, q.d) / 2 + 0.5, std::max(q.w, q.d) * 0.18, q.h, 0.4, 1.3, gold));
            else append(fs, boxFaces(xy[0], xy[1], b.yaw, q.w + 0.3, q.d + 0.3, q.h, q.h + 0.3, band));
            return fs;
        });
        tops.push_back({ xy[0], xy[1], z + q.h });
        if (q.main) append(f, beamFaces({ xy[0], xy[1], z + q.h + 1.2 }, { xy[0], xy[1], z + q.h + 2.4 }, 0.14, 0.14, gold));
    }
    // escadas do principal até cada casa
    for (size_t i = 1; i < tops.size(); i++) {
        const V3 &a = tops[0], &q = tops[i];
        const double za = a[2] - bl[0].h, zq = q[2] - bl[i].h;
        append(f, beamFaces({ a[0] + (q[0] - a[0]) * 0.3, a[1] + (q[1] - a[1]) * 0.3, za + 0.1 }, { a[0] + (q[0] - a[0]) * 0.7, a[1] + (q[1] - a[1]) * 0.7, zq + 0.1 }, 0.7, 0.16, found));
    }
    drawFaces(p, f, cam);
    // bandeirolas de oração: cordas com retalhos coloridos entre os telhados
    static const RGB fl[5] = { { 60, 110, 196 }, { 244, 244, 240 }, { 200, 60, 52 }, { 70, 150, 90 }, { 236, 196, 70 } };
    p.save();
    for (size_t i = 1; i < tops.size(); i++) {
        const QPointF a = cam.proj(tops[0][0], tops[0][1], tops[0][2] + 1.6), q = cam.proj(tops[i][0], tops[i][1], tops[i][2] + 0.5);
        const double dl = std::hypot(q.x() - a.x(), q.y() - a.y());
        const QPointF mid((a.x() + q.x()) / 2, (a.y() + q.y()) / 2 + dl * 0.12);
        QPainterPath cord; cord.moveTo(a); cord.quadTo(mid, q);
        p.setBrush(Qt::NoBrush); p.setPen(pen(QColor(60, 50, 40, 153), 0.6)); p.drawPath(cord);
        if (!cam.detail) continue;
        const int n = 7;
        const double sz = std::max(1.2, dl / 22);
        for (int j = 1; j < n; j++) {
            const double t = double(j) / n, X = (1 - t) * (1 - t) * a.x() + 2 * (1 - t) * t * mid.x() + t * t * q.x(), Y = (1 - t) * (1 - t) * a.y() + 2 * (1 - t) * t * mid.y() + t * t * q.y();
            p.fillRect(QRectF(X - sz / 2, Y, sz, sz * 1.3), rgb(fl[(j + i) % 5]));
        }
    }
    p.restore();
}

// Palácio: corpo simétrico creme, pórtico de colunas com frontão, a cúpula sobre o tambor no meio, as alas e os
// pavilhões das pontas; jardim de canteiros e o espelho d'água na frente
void drawPalace(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB creams[3] = { { 238, 230, 212 }, { 232, 224, 206 }, { 242, 236, 222 } }, roofs[3] = { { 96, 104, 118 }, { 120, 92, 74 }, { 86, 98, 92 } };
    static const RGB domes[3] = { { 108, 156, 140 }, { 214, 178, 90 }, { 120, 128, 146 } };
    const RGB cream = creams[pickOf(b.v, 3, 3)], roof = roofs[pickOf(b.v, 5, 3)], dome = domes[pickOf(b.v, 7, 3)], gold = { 214, 178, 96 }, winC = { 70, 76, 92 };
    Faces f, ground;
    // jardim
    auto rect = [&](double x0, double y0, double x1, double y1, double z, const RGB& col, Deco deco) {
        ground.push_back(flatPoly({ L(x0, y0, z), L(x1, y0, z), L(x1, y1, z), L(x0, y1, z) }, col, L(0, 0, -1), std::move(deco)));
    };
    rect(2.6, -8.5, 9.8, 8.5, 0.05, { 194, 184, 160 }, Deco());
    for (int s : { -1, 1 })
        rect(3.4, s * 1.6, 9.2, s * 7.6, 0.08, { 98, 144, 76 }, [](QPainter& q, const PtFn& pt, double) {
            strokeLines(q, QColor(214, 200, 160, 230), 1); line(q, pt(0.5, 0), pt(0.5, 1)); line(q, pt(0, 0.5), pt(1, 0.5));
        });
    rect(3.6, -0.9, 9.0, 0.9, 0.1, { 110, 160, 196 }, [](QPainter& q, const PtFn& pt, double) { strokeLines(q, QColor(255, 255, 255, 128), 0.6); line(q, pt(0.1, 0.3), pt(0.5, 0.3)); });
    // corpo central, alas e pavilhões
    auto block = [&](double lx, double ly, double hx, double hy, double h, int rows) {
        const P2 xy = L.at(lx, ly);
        Faces fs = boxFaces(xy[0], xy[1], b.yaw, hx * 2, hy * 2, 0, h, cream, nullptr, [hx, hy, rows, winC](int k, double) {
            return windowRows(rows, std::max(2, static_cast<int>(std::lround((k % 2 ? hx : hy) * 2 / 1.25))), winC);
        });
        fs.pop_back();
        append(f, std::move(fs));
        append(f, hipRoof(L, lx, ly, hx + 0.2, hy + 0.2, h, std::min(hx, hy) * 0.8, roof, roofDeco));
    };
    block(0, 0, 2.2, 3.4, 5.0, 2);
    for (int s : { -1, 1 }) { block(-0.3, s * 6.0, 1.6, 2.4, 4.0, 2); block(0.4, s * 8.6, 2.0, 1.0, 4.6, 2); }
    // pórtico com colunas e frontão
    const double pz = 4.3;
    const P2 po = L.at(2.9, 0);
    append(f, boxFaces(po[0], po[1], b.yaw, 1.4, 4.4, 0, 0.45, shade(cream, 0.95)));
    for (int i = 0; i < 6; i++) { const P2 q = L.at(3.3, -1.8 + i * 0.72); append(f, taperFaces(q[0], q[1], 0.2, 0.17, 0.45, pz, 8, shade(cream, 1.04))); }
    append(f, boxFaces(po[0], po[1], b.yaw, 1.4, 4.6, pz, pz + 0.5, cream));
    { Face t = face({ L(3.6, -2.3, pz + 0.5), L(3.6, 2.3, pz + 0.5), L(3.6, 0, pz + 1.6) }, shade(cream, 0.98), L(2.9, 0, pz)); t.q = { 0, 1, 2, 2 }; f.push_back(std::move(t)); }
    f.push_back(face({ L(2.2, -2.3, pz + 0.5), L(3.6, -2.3, pz + 0.5), L(3.6, 0, pz + 1.6), L(2.2, 0, pz + 1.6) }, roof, L(2.9, 0, pz), Deco(), 0.01));
    f.push_back(face({ L(3.6, 2.3, pz + 0.5), L(2.2, 2.3, pz + 0.5), L(2.2, 0, pz + 1.6), L(3.6, 0, pz + 1.6) }, roof, L(2.9, 0, pz), Deco(), 0.01));
    // tambor e cúpula
    const double dz = 5.0 + 1.4;
    append(f, taperFaces(b.x, b.y, 1.9, 1.9, 4.6, dz + 1.3, 14, cream, [winC](int j) { return j % 2 ? roundWin(1, winC, 0.25, 0.75) : Deco(); }));
    append(f, lathe(L, 0, 0, { { 2.05, dz + 1.3 }, { 2.0, dz + 2.1 }, { 1.6, dz + 3.0 }, { 0.9, dz + 3.6 }, { 0, dz + 3.8 } }, 14, dome));
    append(f, taperFaces(b.x, b.y, 0.4, 0.35, dz + 3.75, dz + 4.6, 8, cream));
    append(f, coneFaces(b.x, b.y, 0.45, dz + 4.6, 0.6, 8, gold));
    drawFaces(p, ground, cam);
    drawFaces(p, f, cam);
}

// Estalagem: casa de dois andares de enxaimel em L, a placa pendurada, o estábulo de telheiro e o poço
void drawInn(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB roofs[3] = { { 150, 72, 54 }, { 112, 84, 66 }, { 92, 96, 110 } };
    const RGB wall = { 228, 214, 186 }, beam = { 92, 64, 42 }, roof = roofs[pickOf(b.v, 3, 3)];
    Faces f;
    auto timber = [beam](int rows, int n) -> Deco {
        return [beam, rows, n](QPainter& q, const PtFn& pt, double kk) {
            windowRows(rows, n, DARK)(q, pt, kk);
            strokeLines(q, rgb(beam, kk, 0.9), 1);
            line(q, pt(0, 0.98), pt(1, 0.98)); line(q, pt(0, 0.5), pt(1, 0.5)); line(q, pt(0.02, 0), pt(0.02, 1)); line(q, pt(0.98, 0), pt(0.98, 1));
            for (int i = 1; i < n; i++) line(q, pt(double(i) / n, 0.5), pt(double(i) / n - 0.08, 0.98));
        };
    };
    auto wing = [&](double lx, double ly, double w, double d, double h, bool rotate) {
        const P2 xy = L.at(lx, ly);
        const double yaw = b.yaw + (rotate ? M_PI / 2 : 0);
        const Loc Lw = locAt(xy[0], xy[1], yaw);
        Faces fs = boxFaces(xy[0], xy[1], yaw, w, d, 0, h, wall, nullptr, [&](int k, double) { return timber(2, std::max(2, static_cast<int>(std::lround((k % 2 ? d : w) / 1.4)))); });
        fs.pop_back();
        append(f, std::move(fs));
        append(f, gableFaces(Lw, 0, 0, w / 2, d / 2, h, 1.9, wall, roof));
        const P2 c = Lw.at(w * 0.3, d * 0.2);
        const RGB chTop = { 70, 56, 48 };
        for (Face& q : boxFaces(c[0], c[1], yaw, 0.6, 0.6, h + 0.6, h + 2.4, { 150, 120, 100 }, &chTop)) { q.bias = 0.03; f.push_back(std::move(q)); }
    };
    wing(0, -0.6, 5.0, 3.0, 3.8, false);
    wing(-1.6, 2.6, 3.6, 2.6, 3.4, true);
    // estábulo de telheiro aberto
    const P2 sp = L.at(1.4, 3.6);
    for (const P2& d : { P2{ -1.3, -0.9 }, P2{ 1.3, -0.9 }, P2{ -1.3, 0.9 }, P2{ 1.3, 0.9 } }) {
        const P2 q = L.at(1.4 + d[0], 3.6 + d[1]);
        append(f, beamFaces({ q[0], q[1], 0 }, { q[0], q[1], d[1] < 0 ? 2.1 : 1.6 }, 0.22, 0.22, beam));
    }
    f.push_back(face({ L(0.0, 2.6, 2.15), L(2.8, 2.6, 2.15), L(2.8, 4.6, 1.6), L(0.0, 4.6, 1.6) }, { 176, 150, 98 }, { sp[0], sp[1], 0 }, roofDeco, 0.02));
    { const P2 q = L.at(1.4, 4.0); append(f, boxFaces(q[0], q[1], b.yaw, 2.2, 0.8, 0, 0.6, { 196, 170, 110 })); }
    // placa pendurada
    const P2 pp = L.at(2.8, -2.4);
    append(f, beamFaces({ pp[0], pp[1], 0 }, { pp[0], pp[1], 3.0 }, 0.18, 0.18, beam));
    append(f, beamFaces({ pp[0], pp[1], 2.9 }, L(2.8, -1.4, 2.9), 0.14, 0.14, beam));
    append(f, plate({ L(2.8, -2.15, 2.75), L(2.8, -1.65, 2.75), L(2.8, -1.65, 2.0), L(2.8, -2.15, 2.0) }, dirL(b, 1, 0), 0.08, { 176, 120, 60 }, RGB{ 150, 100, 50 }));
    // poço
    const P2 wp = L.at(3.2, 1.6);
    append(f, taperFaces(wp[0], wp[1], 0.65, 0.65, 0, 0.7, 10, { 176, 168, 152 }));
    for (int s : { -1, 1 }) append(f, beamFaces({ wp[0], wp[1] + s * 0.6, 0.6 }, { wp[0], wp[1] + s * 0.6, 1.8 }, 0.12, 0.12, beam));
    append(f, frustum(locAt(wp[0], wp[1], 0), 0, 0, 0.75, 0.85, 0.05, 0.85, 1.7, 2.3, roof));
    drawFaces(p, f, cam);
}

// Moinho d'Água: casa de pedra com o andar de madeira, a roda d'água de pás no lado e o canal
void drawWatermill(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB roofs[2] = { { 150, 72, 54 }, { 112, 84, 66 } };
    const RGB stone = kHStones[pickOf(b.v, 3, 3)], wood = { 140, 104, 70 }, roof = roofs[pickOf(b.v, 2, 2)];
    Faces f, ground;
    ground.push_back(flatPoly({ L(-4.2, 2.0, 0.06), L(4.2, 2.0, 0.06), L(4.2, 3.8, 0.06), L(-4.2, 3.8, 0.06) }, { 104, 150, 186 }, L(0, 0, -1), [](QPainter& q, const PtFn& pt, double) {
        strokeLines(q, QColor(255, 255, 255, 140), 0.7);
        for (int i = 0; i < 4; i++) line(q, pt(0.1 + i * 0.22, 0.3 + (i % 2) * 0.35), pt(0.18 + i * 0.22, 0.3 + (i % 2) * 0.35));
    }));
    for (int s : { -1, 1 }) { const P2 q = L.at(0, 2.9 + s * 1.0); append(f, boxFaces(q[0], q[1], b.yaw, 8.4, 0.25, -0.3, 0.35, shade(stone, 0.9))); }
    Faces fs = boxFaces(b.x, b.y, b.yaw, 4.6, 3.6, 0, 2.2, stone, nullptr, [](int k, double) { return k % 2 == 0 ? smallWin(2) : Deco(stoneCoursesDeco); });
    fs.pop_back();
    append(f, std::move(fs));
    Faces fs2 = boxFaces(b.x, b.y, b.yaw, 4.6, 3.6, 2.2, 3.8, wood, nullptr, [](int k, double) { return k % 2 == 0 ? smallWin(2, 0.3, 0.7) : Deco(); });
    fs2.pop_back();
    append(f, std::move(fs2));
    append(f, gableFaces(L, 0, 0, 2.3, 1.8, 3.8, 1.9, wood, roof));
    // roda: dois aros, o eixo e as pás
    const V3 nrm = dirL(b, 0, 1);
    const double rr = 1.9;
    append(f, disc(L(0.3, 2.15, 1.9), nrm, rr, 0.08, 16, wood, shade(wood, 0.8)));
    append(f, disc(L(0.3, 2.95, 1.9), nrm, rr, 0.08, 16, wood, shade(wood, 0.8)));
    append(f, beamFaces(L(0.3, 1.7, 1.9), L(0.3, 3.2, 1.9), 0.3, 0.3, shade(wood, 0.7)));
    for (int i = 0; i < 10; i++) {
        const double a = i / 10.0 * M_PI * 2, ca = std::cos(a), sa = std::sin(a);
        append(f, beamFaces(L(0.3 + ca * 0.4, 2.15, 1.9 + sa * 0.4), L(0.3 + ca * rr, 2.15, 1.9 + sa * rr), 0.12, 0.12, shade(wood, 0.9)));
        append(f, plate({ L(0.3 + ca * (rr - 0.6), 2.15, 1.9 + sa * (rr - 0.6)), L(0.3 + ca * (rr - 0.6), 2.95, 1.9 + sa * (rr - 0.6)),
                          L(0.3 + ca * (rr + 0.05), 2.95, 1.9 + sa * (rr + 0.05)), L(0.3 + ca * (rr + 0.05), 2.15, 1.9 + sa * (rr + 0.05)) },
                        { std::cos(b.yaw) * -sa, std::sin(b.yaw) * -sa, ca }, 0.08, { 156, 120, 82 }));
    }
    drawFaces(p, ground, cam);
    drawFaces(p, f, cam);
}

// Observatório: torre redonda de pedra, a cúpula de cobre com a fresta aberta e a luneta apontando pro céu
void drawObservatory(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB coppers[3] = { { 96, 150, 136 }, { 180, 120, 80 }, { 128, 138, 150 } };
    const RGB stone = kHStones[pickOf(b.v, 3, 3)], copper = coppers[pickOf(b.v, 3, 3)];
    Faces f = taperFaces(b.x, b.y, 2.5, 2.25, 0, 6.2, 14, stone, [](int j) { return j % 4 == 0 ? smallWin(1, 0.55, 0.8) : j % 4 == 2 ? smallWin(1, 0.2, 0.4) : Deco(); });
    append(f, ringL(L, 0, 0, 2.55, 2.55, 2.55, 2.55, 6.2, 6.6, 14, shade(stone, 0.9)));
    append(f, lathe(L, 0, 0, { { 2.35, 6.6 }, { 2.3, 7.3 }, { 1.95, 8.1 }, { 1.3, 8.7 }, { 0.6, 9.0 }, { 0, 9.05 } }, 14, copper));
    // a fresta: faixa escura na frente da cúpula, de baixo até o topo
    std::vector<P2> slit;
    for (int i = 0; i <= 6; i++) { const double t = i / 6.0, a = t * M_PI / 2 * 0.98; slit.push_back({ 2.38 * std::cos(a) + 0.02, 6.6 + std::sin(a) * 2.45 }); }
    for (int i = 0; i < 6; i++) {
        const double r0 = slit[i][0], z0 = slit[i][1], r1 = slit[i + 1][0], z1 = slit[i + 1][1];
        f.push_back(face({ L(r0 + 0.02, -0.42, z0), L(r0 + 0.02, 0.42, z0), L(r1 + 0.02, 0.42, z1), L(r1 + 0.02, -0.42, z1) }, { 40, 36, 40 }, L(0, 0, 7.5), Deco(), 0.03));
    }
    append(f, limb(L(0.6, 0, 7.4), L(3.2, 0, 9.6), 0.34, 0.28, 8, { 70, 64, 60 }));
    append(f, limb(L(3.2, 0, 9.6), L(3.5, 0, 9.85), 0.38, 0.38, 8, { 196, 170, 100 }));
    // anexo baixo encostado
    const P2 ap = L.at(-2.6, 1.6);
    Faces fs = boxFaces(ap[0], ap[1], b.yaw, 2.4, 2.2, 0, 2.2, stone, nullptr, [](int k, double) { return k % 2 ? Deco() : smallWin(1); });
    fs.pop_back();
    append(f, std::move(fs));
    append(f, gableFaces(locAt(ap[0], ap[1], b.yaw), 0, 0, 1.2, 1.1, 2.2, 1.1, stone, { 96, 104, 118 }));
    drawFaces(p, f, cam);
}

// Academia: quatro alas em volta do claustro gramado, janelas ogivais, telhado de ardósia e a torre do relógio na frente
void drawAcademy(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    static const RGB stones[3] = { { 206, 192, 164 }, { 196, 184, 160 }, { 214, 200, 172 } };
    const RGB stone = stones[pickOf(b.v, 3, 3)], slate = { 84, 92, 106 }, winC = { 64, 70, 86 };
    Faces f, ground;
    ground.push_back(flatPoly({ L(-3.2, -3.2, 0.06), L(3.2, -3.2, 0.06), L(3.2, 3.2, 0.06), L(-3.2, 3.2, 0.06) }, { 104, 150, 82 }, L(0, 0, -1), [](QPainter& q, const PtFn& pt, double) {
        strokeLines(q, QColor(214, 200, 160, 242), 1.1); line(q, pt(0.5, 0), pt(0.5, 1)); line(q, pt(0, 0.5), pt(1, 0.5));
    }));
    auto wing = [&](double lx, double ly, double w, double d, double h) {
        const P2 xy = L.at(lx, ly);
        Faces fs = boxFaces(xy[0], xy[1], b.yaw, w, d, 0, h, stone, nullptr, [w, d, winC](int k, double) { return lancet(std::max(2, static_cast<int>(std::lround((k % 2 ? d : w) / 1.3))), winC); });
        fs.pop_back();
        append(f, std::move(fs));
        append(f, hipRoof(L, lx, ly, w / 2 + 0.2, d / 2 + 0.2, h, std::min(w, d) * 0.55, slate, roofDeco));
    };
    wing(4.4, 0, 2.4, 11.2, 4.0); wing(-4.4, 0, 2.4, 11.2, 4.6);
    wing(0, 4.4, 6.4, 2.4, 3.8); wing(0, -4.4, 6.4, 2.4, 3.8);
    // torre do relógio no meio da ala da frente
    const P2 tp = L.at(5.0, 0);
    const double th = 9.0;
    append(f, boxFaces(tp[0], tp[1], b.yaw, 2.4, 2.4, 0, th, stone, nullptr, [winC](int k, double) -> Deco {
        return [k, winC](QPainter& q, const PtFn& pt, double kk) {
            lancet(1, winC, 0.15, 0.45)(q, pt, kk);
            if (k != 0) return;
            const QPointF c = pt(0.5, 0.78), e = pt(0.82, 0.78);
            const double r = std::max(1.0, std::fabs(e.x() - c.x()));
            q.setBrush(rgb({ 240, 234, 214 }, kk)); q.setPen(pen(rgb(DARK, kk, 0.8), 0.7)); q.drawEllipse(c, r, r);
            q.setBrush(Qt::NoBrush);
            line(q, c, QPointF(c.x(), c.y() - r * 0.7)); line(q, c, QPointF(c.x() + r * 0.5, c.y()));
        };
    }));
    append(f, boxFaces(tp[0], tp[1], b.yaw, 2.7, 2.7, th, th + 0.4, shade(stone, 0.92)));
    append(f, frustum(L, 5.0, 0, 1.25, 1.25, 0, 0, th + 0.4, th + 4.2, slate));
    for (const P2& s : { P2{ 1, 1 }, P2{ 1, -1 }, P2{ -1, 1 }, P2{ -1, -1 } }) { const P2 q = L.at(5.0 + s[0] * 1.2, s[1] * 1.2); append(f, coneFaces(q[0], q[1], 0.25, th + 0.4, 1.2, 6, slate)); }
    drawFaces(p, ground, cam);
    drawFaces(p, f, cam);
}

// Posto de Vigia: torre de toras em quatro pernas com travessas, plataforma, telhadinho e o braseiro
void drawWatchtower(QPainter& p, const Art& b, const Cam& cam)
{
    static const RGB LOGc = { 128, 94, 62 };
    static const int cn[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
    const double h = 7.4, s0 = 1.5, s1 = 0.95;
    const Loc L(b);
    auto leg = [&](double sx, double sy, double t) { return L(sx * (s0 + (s1 - s0) * t), sy * (s0 + (s1 - s0) * t), h * t); };
    Faces f;
    for (const auto& c : cn) append(f, beamFaces(leg(c[0], c[1], 0), leg(c[0], c[1], 1.12), 0.3, 0.3, LOGc));
    for (double t : { 0.25, 0.6 })
        for (int k = 0; k < 4; k++) append(f, beamFaces(leg(cn[k][0], cn[k][1], t - 0.12), leg(cn[(k + 1) % 4][0], cn[(k + 1) % 4][1], t + 0.12), 0.16, 0.16, shade(LOGc, 0.92)));
    append(f, boxFaces(b.x, b.y, b.yaw, s1 * 2 + 0.9, s1 * 2 + 0.9, h - 0.3, h, { 150, 116, 78 }));
    for (Face& q : boxFaces(b.x, b.y, b.yaw, s1 * 2 + 0.9, s1 * 2 + 0.9, h, h + 0.8, { 140, 106, 70 }, nullptr, [](int, double) -> Deco {
             return [](QPainter& qq, const PtFn& pt, double) { strokeLines(qq, QColor(52, 34, 20, 128), 0.6); for (int i = 1; i < 5; i++) line(qq, pt(i / 5.0, 0), pt(i / 5.0, 1)); };
         }))
        if (q.v.size() == 4 && q.v[0][2] != q.v[2][2]) f.push_back(std::move(q));     // só o parapeito, sem tampa
    append(f, frustum(L, 0, 0, s1 + 0.7, s1 + 0.7, 0, 0, h + 2.0, h + 3.4, { 110, 84, 60 }));
    for (const auto& c : cn) append(f, beamFaces(L(c[0] * s1 * 0.95, c[1] * s1 * 0.95, h), L(c[0] * s1 * 0.95, c[1] * s1 * 0.95, h + 2.05), 0.16, 0.16, LOGc));
    // escada encostada
    append(f, beamFaces(L(s0 + 1.4, -0.35, 0), L(s1 + 0.35, -0.35, h), 0.12, 0.12, LOGc));
    append(f, beamFaces(L(s0 + 1.4, 0.35, 0), L(s1 + 0.35, 0.35, h), 0.12, 0.12, LOGc));
    drawFaces(p, f, cam);
    flame(p, cam, L(0, 0, h + 0.75), 0.5);
}

// Santuário: caminho de pedra com portais vermelhos em fila, lanternas de pedra e o salão de beiral curvo
void drawShrine(QPainter& p, const Art& b, const Cam& cam)
{
    const Loc L(b);
    const RGB verm = { 214, 72, 46 }, blk = { 40, 36, 34 }, stone = { 176, 172, 162 };
    Faces f, ground;
    ground.push_back(flatPoly({ L(-2.4, -0.9, 0.06), L(6.8, -0.9, 0.06), L(6.8, 0.9, 0.06), L(-2.4, 0.9, 0.06) }, { 192, 186, 172 }, L(0, 0, -1), [](QPainter& q, const PtFn& pt, double) {
        strokeLines(q, QColor(90, 84, 74, 102), 0.6);
        for (int i = 1; i < 10; i++) line(q, pt(i / 10.0, 0), pt(i / 10.0, 1));
    }));
    auto torii = [&](double lx, double s) {
        const double hw = 1.5 * s, h = 3.4 * s;
        for (int sd : { -1, 1 }) {
            append(f, limb(L(lx, sd * hw, 0), L(lx, sd * hw * 0.94, h), 0.2 * s, 0.17 * s, 8, verm));
            append(f, limb(L(lx, sd * hw, 0), L(lx, sd * hw, 0.35), 0.24 * s, 0.24 * s, 8, blk));
        }
        append(f, beamFaces(L(lx, -hw - 0.35 * s, h * 0.78), L(lx, hw + 0.35 * s, h * 0.78), 0.22 * s, 0.24 * s, verm));
        append(f, beamFaces(L(lx, -hw - 0.7 * s, h + 0.05), L(lx, 0, h + 0.18 * s), 0.3 * s, 0.26 * s, blk));
        append(f, beamFaces(L(lx, 0, h + 0.18 * s), L(lx, hw + 0.7 * s, h + 0.05), 0.3 * s, 0.26 * s, blk));
        append(f, beamFaces(L(lx, -hw - 0.6 * s, h - 0.2), L(lx, hw + 0.6 * s, h - 0.2), 0.26 * s, 0.22 * s, verm));
    };
    torii(6.2, 1.15); torii(4.4, 1.0); torii(2.8, 0.92);
    // lanternas de pedra dos dois lados do caminho
    for (int s : { -1, 1 }) {
        const P2 q = L.at(1.4, s * 1.6);
        append(f, taperFaces(q[0], q[1], 0.32, 0.3, 0, 0.3, 6, stone));
        append(f, taperFaces(q[0], q[1], 0.12, 0.12, 0.3, 1.0, 6, stone));
        append(f, boxFaces(q[0], q[1], b.yaw, 0.6, 0.6, 1.0, 1.45, stone, nullptr, [](int, double) { return smallWin(1, 0.25, 0.8, { 250, 214, 140 }); }));
        append(f, coneFaces(q[0], q[1], 0.55, 1.45, 0.45, 4, shade(stone, 0.9)));
    }
    // o salão sobre a plataforma
    const P2 hp = L.at(-1.0, 0);
    append(f, boxFaces(hp[0], hp[1], b.yaw, 3.6, 3.6, 0, 0.5, stone));
    Faces fs = boxFaces(hp[0], hp[1], b.yaw, 2.8, 2.8, 0.5, 2.3, { 236, 228, 212 }, nullptr, [](int k, double) -> Deco {
        if (k == 0) return [](QPainter& q, const PtFn& pt, double kk) { quad(q, pt, 0.3, 0.7, 0, 0.8, rgb({ 120, 70, 50 }, kk)); };
        return smallWin(2);
    });
    fs.pop_back();
    append(f, std::move(fs));
    for (const P2& s : { P2{ 1, 1 }, P2{ 1, -1 }, P2{ -1, 1 }, P2{ -1, -1 } }) { const P2 q = L.at(-1.0 + s[0] * 1.4, s[1] * 1.4); append(f, limb({ q[0], q[1], 0.5 }, { q[0], q[1], 2.3 }, 0.13, 0.13, 6, verm)); }
    append(f, curvedRoof(locAt(hp[0], hp[1], b.yaw), 0, 0, 2.5, 0.5, 2.3, 0.45, 1.6, { 70, 74, 80 }));
    drawFaces(p, ground, cam);
    drawFaces(p, f, cam);
}

// os artefatos da leva 5 (false = não é deles)
bool art5(QPainter& p, const Art& a, const Cam& cam)
{
    const QString& k = a.kind;
    if (k == QLatin1String("equestrian")) drawEquestrian(p, a, cam);
    else if (k == QLatin1String("angel")) drawAngel(p, a, cam);
    else if (k == QLatin1String("sphinx")) drawSphinx(p, a, cam);
    else if (k == QLatin1String("totem")) drawTotems(p, a, cam);
    else if (k == QLatin1String("fallenhead")) drawFallenHead(p, a, cam);
    else if (k == QLatin1String("monastery")) drawMonastery(p, a, cam);
    else if (k == QLatin1String("palace")) drawPalace(p, a, cam);
    else if (k == QLatin1String("inn")) drawInn(p, a, cam);
    else if (k == QLatin1String("watermill")) drawWatermill(p, a, cam);
    else if (k == QLatin1String("observatory")) drawObservatory(p, a, cam);
    else if (k == QLatin1String("academy")) drawAcademy(p, a, cam);
    else if (k == QLatin1String("watchtower")) drawWatchtower(p, a, cam);
    else if (k == QLatin1String("shrine")) drawShrine(p, a, cam);
    else return false;
    return true;
}

// ════════════════════════ peças dos pincéis ════════════════════════
const RGB kCanvasCols[4] = { { 232, 222, 196 }, { 216, 202, 172 }, { 200, 188, 156 }, { 226, 214, 190 } };
const RGB kFelt[3] = { { 230, 222, 202 }, { 214, 200, 176 }, { 196, 178, 152 } };
Loc locH(const House& o) { return locAt(o.x, o.y, o.yaw); }

Faces tentFaces(const House& o)
{
    const Loc L = locH(o);
    const double w = 2.6, hd = 1.0, h = 1.55, hw = w / 2;
    const RGB col = kCanvasCols[pickOf(o.v, 13, 4)];
    const V3 C = { o.x, o.y, 0.5 };
    Faces f;
    f.push_back(face({ L(-hw, -hd, 0.05), L(hw, -hd, 0.05), L(hw, 0, h), L(-hw, 0, h) }, col, C, [](QPainter& q, const PtFn& pt, double) {
        strokeLines(q, QColor(80, 64, 44, 77), 0.6); line(q, pt(0.5, 0), pt(0.5, 1));
    }));
    f.push_back(face({ L(hw, hd, 0.05), L(-hw, hd, 0.05), L(-hw, 0, h), L(hw, 0, h) }, col, C));
    {
        Face t = face({ L(hw, -hd, 0.05), L(hw, hd, 0.05), L(hw, 0, h) }, shade(col, 0.96), C, [](QPainter& q, const PtFn& pt, double kk) {
            QPainterPath pa; pa.moveTo(pt(0.38, 0)); pa.lineTo(pt(0.5, 0.75)); pa.lineTo(pt(0.62, 0)); pa.closeSubpath();
            q.fillPath(pa, rgb({ 84, 66, 46 }, kk, 0.8));
        });
        t.q = { 0, 1, 2, 2 };
        f.push_back(std::move(t));
    }
    { Face t = face({ L(-hw, hd, 0.05), L(-hw, -hd, 0.05), L(-hw, 0, h) }, shade(col, 0.96), C); t.q = { 0, 1, 2, 2 }; f.push_back(std::move(t)); }
    return f;
}
Faces pavilionFaces(const House& o)
{
    const Loc L = locH(o);
    const RGB c1 = { 176, 58, 48 }, c2 = { 236, 228, 208 };
    const Deco fillC1 = [c1](QPainter& q, const PtFn& pt, double kk) { quad(q, pt, 0, 1, 0, 1, rgb(c1, kk)); };
    Faces f = ringL(L, 0, 0, 1.9, 1.9, 1.9, 1.9, 0, 1.4, 12, c2, [fillC1](int k, int) { return k % 2 ? fillC1 : Deco(); });
    append(f, lathe(L, 0, 0, { { 2.1, 1.35 }, { 1.2, 2.2 }, { 0, 2.9 } }, 12, c2, [fillC1](int i, int k) { return i == 0 && k % 2 ? fillC1 : Deco(); }));
    append(f, beamFaces({ o.x, o.y, 2.8 }, { o.x, o.y, 3.7 }, 0.1, 0.1, { 70, 52, 36 }));
    return f;
}
Faces yurtFaces(const House& o)
{
    const Loc L = locH(o);
    static const RGB trims[3] = { { 176, 60, 52 }, { 52, 92, 150 }, { 196, 140, 60 } };
    const RGB col = kFelt[pickOf(o.v, 11, 3)], trim = trims[pickOf(o.v, 7, 3)];
    Faces f = ringL(L, 0, 0, 1.4, 1.4, 1.36, 1.36, 0, 1.05, 12, col, [trim](int k, int) -> Deco {
        return [trim, k](QPainter& q, const PtFn& pt, double kk) {
            quad(q, pt, 0, 1, 0.78, 0.92, rgb(trim, kk, 0.9));
            if (k == 0) quad(q, pt, 0.25, 0.75, 0, 0.62, rgb({ 200, 120, 50 }, kk));
        };
    });
    append(f, lathe(L, 0, 0, { { 1.45, 1.02 }, { 1.0, 1.5 }, { 0.35, 1.78 }, { 0.35, 1.86 }, { 0, 1.86 } }, 12, shade(col, 0.93)));
    return f;
}
Faces ruinFaces(const House& o)
{
    const Loc L = locH(o);
    const double seed0 = std::floor(o.v * 1e6);
    Rng R(seed0 + 3);
    const int kind = pickOf(o.v, 5, 5);
    const RGB stone = kHStones[pickOf(o.v, 7, 3)], moss = { 128, 140, 96 };
    auto pick = [&]() { return R() < 0.25 ? mix(stone, moss, 0.5) : shade(stone, 0.92 + R() * 0.12); };
    const Deco seam = [](QPainter& q, const PtFn& pt, double) { strokeLines(q, QColor(60, 54, 48, 77), 0.6); line(q, pt(0.5, 0), pt(0.5, 1)); };
    Faces f;
    if (kind == 0) {          // pedaço de muro com o topo quebrado
        const double len = 3.0 + R() * 1.6, th = 0.8;
        const int n = 7;
        std::vector<P2> top;
        for (int i = 0; i <= n; i++) {
            const double x = -len / 2 + len * i / n;
            const double z = 1.0 + R() * 2.4 * std::sin(double(i) / n * M_PI) + R() * 0.6;
            top.push_back({ x, z });
        }
        for (int sd : { -1, 1 }) {
            std::vector<V3> v = { L(-len / 2, sd * th / 2, -0.3), L(len / 2, sd * th / 2, -0.3) };
            for (int i = n; i >= 0; i--) v.push_back(L(top[i][0], sd * th / 2, top[i][1]));
            if (sd < 0) std::reverse(v.begin(), v.end());
            const int nv = static_cast<int>(v.size());
            Face fc = face(v, stone, { o.x, o.y, 1 }, Deco(stoneCoursesDeco));
            fc.q = sd < 0 ? std::array<int, 4>{ nv - 1, nv - 2, nv - 3, 0 } : std::array<int, 4>{ 0, 1, 2, nv - 1 };
            f.push_back(std::move(fc));
        }
        for (int i = 0; i < n; i++)
            f.push_back(face({ L(top[i][0], -th / 2, top[i][1]), L(top[i + 1][0], -th / 2, top[i + 1][1]), L(top[i + 1][0], th / 2, top[i + 1][1]), L(top[i][0], th / 2, top[i][1]) }, pick(), { o.x, o.y, -2 }));
        const P2 q = L.at(len * 0.3, 1.2);
        const RGB pc = pick();
        append(f, blobFaces(q[0], q[1], 0.2, 0.6, 0.4, 5, 2, pc, seed0 + 5, 0.3));
    } else if (kind == 1) {   // colunas em pé, uma inteira com o capitel
        const int n = 2 + static_cast<int>(std::floor(R() * 2));
        for (int i = 0; i < n; i++) {
            const P2 q = L.at((i - (n - 1) / 2.0) * 1.6, 0);
            const double h = i == 0 ? 4.0 : 1.2 + R() * 2.4;
            append(f, boxFaces(q[0], q[1], o.yaw, 0.95, 0.95, 0, 0.35, stone));
            const RGB pc = pick();
            append(f, taperFaces(q[0], q[1], 0.36, 0.32, 0.35, h, 8, pc, [seam](int j) { return j % 2 ? seam : Deco(); }));
            if (i == 0) append(f, boxFaces(q[0], q[1], o.yaw, 1.0, 1.0, h, h + 0.4, stone));
        }
    } else if (kind == 2) {   // coluna tombada em tambores
        const double yaw = o.yaw + 0.3, dx = std::cos(yaw), dy = std::sin(yaw);
        for (int i = 0; i < 3; i++) {
            const P2 q = L.at(-1.8 + i * 1.25, 0.1 * i);
            const RGB pc = pick();
            append(f, limb({ q[0], q[1], 0.35 }, { q[0] + dx * 1.1, q[1] + dy * 1.1, 0.35 }, 0.38, 0.38, 8, pc));
        }
        const P2 q = L.at(-2.6, -0.4);
        append(f, boxFaces(q[0], q[1], o.yaw, 0.95, 0.95, 0, 0.6, stone));
    } else if (kind == 3) {   // entulho
        for (int i = 0; i < 5; i++) {
            const double a = R() * M_PI * 2, d = R() * 1.6;
            const P2 q = L.at(std::cos(a) * d, std::sin(a) * d);
            const double rx = 0.45 + R() * 0.4;
            const double rz = 0.35 + R() * 0.3;
            const RGB pc = pick();
            append(f, clampBelow(blobFaces(q[0], q[1], 0.1, rx, rz, 5, 2, pc, seed0 + 7 + i, 0.35)));
        }
    } else {                  // arco quebrado: dois pilares e meio arco
        for (int sd : { -1, 1 }) {
            const P2 q = L.at(0, sd * 1.4);
            const RGB pc = pick();
            append(f, boxFaces(q[0], q[1], o.yaw, 0.8, 0.8, 0, sd < 0 ? 2.8 : 1.9, pc, nullptr, [](int, double) { return Deco(stoneCoursesDeco); }));
        }
        for (int i = 0; i < 5; i++) {
            const double a = M_PI - i / 8.0 * M_PI;
            const P2 q = L.at(0, std::cos(a) * 1.4);
            const RGB pc = pick();
            append(f, boxFaces(q[0], q[1], o.yaw, 0.8, 0.5, 2.8 + std::sin(a) * 1.2 - 0.25, 2.8 + std::sin(a) * 1.2 + 0.3, pc));
        }
    }
    return f;
}
void drawProp5(QPainter& p, const House& o, const Cam& cam)
{
    if (o.prop == QLatin1String("tent")) {
        drawFaces(p, o.big ? pavilionFaces(o) : tentFaces(o), cam);
        if (o.big) flag(p, cam, o.x, o.y, 3.4, { 176, 58, 48 });
    } else if (o.prop == QLatin1String("fire")) {
        const Loc L = locH(o);
        Faces f;
        for (int i = 0; i < 4; i++) {
            const double a = i / 4.0 * M_PI + o.yaw;
            append(f, beamFaces(L(std::cos(a) * 0.7, std::sin(a) * 0.7, 0.1), L(-std::cos(a) * 0.7, -std::sin(a) * 0.7, 0.1), 0.18, 0.18, { 96, 70, 46 }));
        }
        for (int i = 0; i < 7; i++) { const double a = i / 7.0 * M_PI * 2; const P2 q = L.at(std::cos(a) * 0.95, std::sin(a) * 0.95); append(f, boxFaces(q[0], q[1], a, 0.4, 0.3, 0, 0.25, { 150, 144, 132 })); }
        drawFaces(p, f, cam);
        flame(p, cam, { o.x, o.y, 0.15 }, 0.75);
    } else if (o.prop == QLatin1String("yurt")) drawFaces(p, yurtFaces(o), cam);
    else if (o.prop == QLatin1String("ruin")) drawFaces(p, ruinFaces(o), cam);
    else if (o.prop == QLatin1String("pier")) {
        drawDock(p, Dock{ o.x0, o.y0, o.x1, o.y1 }, cam);
        const double dx = o.x1 - o.x0, dy = o.y1 - o.y0;
        double l = std::hypot(dx, dy);
        if (!l) l = 1;
        const double ux = dx / l, uy = dy / l;
        Faces f;
        for (int s : { -1, 1 }) append(f, beamFaces({ o.x1 + uy * s * 0.75, o.y1 - ux * s * 0.75, -0.6 }, { o.x1 + uy * s * 0.75, o.y1 - ux * s * 0.75, 1.2 }, 0.22, 0.22, { 96, 72, 50 }));
        drawFaces(p, f, cam);
    }
}
bool pierBoat5(const House& o, Boat& bt)
{
    if (o.prop != QLatin1String("pier") || !o.boat) return false;
    const double dx = o.x1 - o.x0, dy = o.y1 - o.y0;
    double l = std::hypot(dx, dy);
    if (!l) l = 1;
    const double ux = dx / l, uy = dy / l, sd = o.v > 0.75 ? -1 : 1;
    bt = Boat{ o.x1 - ux * 1.9 + uy * sd * 1.7, o.y1 - uy * 1.9 - ux * sd * 1.7, std::atan2(uy, ux), o.v };
    return true;
}

// pomar, vinhedo e lavanda: retalhos de chão (como a plantação) com as fileiras em pé
void drawField5(QPainter& p, const Field& fd, const Cam& cam)
{
    const double c = std::cos(fd.yaw), s = std::sin(fd.yaw);
    auto Lf = [&](double lx, double ly, double z = 0.05) { return V3{ fd.x + c * lx - s * ly, fd.y + s * lx + c * ly, z }; };
    const double hw = fd.w / 2, hd = fd.d / 2;
    const int crop = fd.crop;
    const RGB soil = crop == 4 ? RGB{ 128, 152, 88 } : crop == 5 ? RGB{ 150, 128, 96 } : RGB{ 140, 120, 100 };
    {
        Face g;
        g.v = { Lf(-hw, -hd), Lf(hw, -hd), Lf(hw, hd), Lf(-hw, hd) };
        g.col = soil; g.flat = true;
        g.deco = [](QPainter& q, const PtFn& pt, double) {
            QPainterPath pa; pa.moveTo(pt(0, 0)); pa.lineTo(pt(1, 0)); pa.lineTo(pt(1, 1)); pa.lineTo(pt(0, 1)); pa.closeSubpath();
            q.setBrush(Qt::NoBrush); q.setPen(pen(QColor(52, 78, 40, 178), 1.2)); q.drawPath(pa);
        };
        drawFaces(p, Faces{ g }, detailed(cam));
    }
    if (crop == 4) {        // pomar: árvores redondinhas em grade
        const int nx = std::max(2, static_cast<int>(std::lround(fd.w / 1.7))), ny = std::max(2, static_cast<int>(std::lround(fd.d / 1.7)));
        const double ca = std::cos(cam.ang), sa = std::sin(cam.ang);
        std::vector<std::pair<V3, double>> pts;
        for (int i = 0; i < nx; i++) for (int j = 0; j < ny; j++) { const V3 q = Lf(-hw + (i + 0.5) * fd.w / nx, -hd + (j + 0.5) * fd.d / ny, 0); pts.push_back({ q, sa * q[0] + ca * q[1] }); }
        std::stable_sort(pts.begin(), pts.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        Rng R(std::floor(fd.v * 1e6) + 21);
        const QColor fruit = fd.v > 0.5 ? QColor(214, 72, 52) : QColor(236, 196, 70);
        p.save();
        for (const auto& pr : pts) {
            const V3& q = pr.first;
            const QPointF X0 = cam.proj(q[0], q[1], 0), X1 = cam.proj(q[0], q[1], 1.3), Xr = cam.proj(q[0] + ca * 0.75, q[1] - sa * 0.75, 1.3);
            const double r = std::max(1.1, std::fabs(Xr.x() - X1.x()) + 0.6);
            p.setPen(pen(QColor(96, 70, 46), std::max(0.6, r * 0.25))); p.drawLine(X0, X1);
            QRadialGradient gr(X1, r * 1.05, QPointF(X1.x() - r * 0.35, X1.y() - r * 0.4), r * 0.1);
            gr.setColorAt(0, QColor(150, 182, 92)); gr.setColorAt(1, QColor(58, 96, 44));
            p.setPen(Qt::NoPen); p.setBrush(gr); p.drawEllipse(X1, r, r);
            if (cam.detail && r > 2.2)
                for (int k = 0; k < 3; k++) {
                    const double a = R() * M_PI * 2, d = R() * r * 0.7, fr = std::max(0.5, r * 0.13);
                    p.setBrush(fruit); p.drawEllipse(QPointF(X1.x() + std::cos(a) * d, X1.y() + std::sin(a) * d), fr, fr);
                }
        }
        p.restore();
        return;
    }
    // vinhedo e lavanda: fileiras compridas em pé
    const int rows = std::max(3, static_cast<int>(std::lround(fd.d / (crop == 5 ? 1.15 : 1.0))));
    const RGB col = crop == 5 ? RGB{ 92, 124, 62 } : RGB{ 150, 118, 192 }, topc = crop == 5 ? RGB{ 112, 146, 72 } : RGB{ 176, 146, 214 };
    Faces f;
    for (int j = 0; j < rows; j++) {
        const double ly = -hd + (j + 0.5) * fd.d / rows;
        const V3 xy = Lf(0, ly);
        if (crop == 5) {
            append(f, boxFaces(xy[0], xy[1], fd.yaw, fd.w - 0.6, 0.42, 0, 0.95, col, &topc, [](int k, double) -> Deco {
                if (k % 2) return Deco();
                return [](QPainter& q, const PtFn& pt, double kk) {      // os cachos
                    const QColor gc = rgb({ 92, 52, 90 }, kk, 0.8);
                    for (int i = 1; i < 8; i++) { const QPointF pp = pt(i / 8.0, 0.35); q.fillRect(QRectF(pp.x() - 0.5, pp.y() - 0.5, 1, 1.2), gc); }
                };
            }));
        } else {
            const Loc Lr = locAt(xy[0], xy[1], fd.yaw);
            append(f, ringL(Lr, 0, 0, fd.w / 2 - 0.3, 0.42, fd.w / 2 - 0.4, 0.2, 0, 0.7, 10, col));
            std::vector<V3> tv;
            for (int k = 0; k < 10; k++) tv.push_back(Lr(std::cos(k / 10.0 * M_PI * 2) * (fd.w / 2 - 0.4), std::sin(k / 10.0 * M_PI * 2) * 0.2, 0.7));
            f.push_back(face(tv, topc, { xy[0], xy[1], 0 }));
        }
    }
    drawFaces(p, f, cam);
}
