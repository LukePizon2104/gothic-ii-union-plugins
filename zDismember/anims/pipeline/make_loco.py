r"""Chod, trucht i sprint Geralta (Wiedzmin 3) w tempie Gothica - TYLKO UZYTEK PRYWATNY (animacje CD Projekt RED).

Jak w W3: dwa cykle tego samego chodu (wolny i szybki) zsynchronizowane faza kroku i zmieszane tak, zeby predkosc
wyszla jak w Gothicu (krok i kadencja razem - stopy nie slizgaja sie). Petle podmieniaja S_RUNL / S_WALKL (bez broni),
S_1HRUNL / S_2HRUNL / ...WALKL (z mieczem, tylko styl W3) i sprint zEBA (gdy wlaczony W3Sprint); przejscia
T_*_2_*L i T_*L_2_* krokami z postawy i do postawy. Tryb magii (czar / runa / Znak w reku): S_MAGRUNL / S_MAGWALKL i ich
przejscia w tych samych nakladkach co bez broni (te same klatki petli, postawa przejsc S_MAGRUN) - rzucanie i
inwestowanie czaru (T_MAGRUN_2_*CAST, S_*CAST, S_*SHOOT, warstwa 1) zostaja z gry, nakladka ich nie ma.

Uzycie: py -3.12 -I make_loco.py [--only NAKLADKA[,NAKLADKA]]
  (zrzuty klipow w anims\work\w3loco, nakladki w data_w3\_work\Data\Anims\_compiled; --only = tylko te nakladki)
"""
import math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import msblib
import retarget, build_overlay, recovery, sfx_events, make_pair, make_moves, make_attacks
from retarget import qmat

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(ROOT, "anims", "work", "w3loco")
COMPILED = make_moves.COMPILED
B = chr(92)
MOV = B.join(["animations", "man", "movement", "man_geralt_movement.w2anims"])
COMBAT = B.join(["animations", "man", "combat", "man_geralt_sword_locomotion.w2anims"])

# Tempo z animacji gry / zEBA (cm/s): bieg 345 (wszystkie style), chod 136-195 (zEBA Remaster 136, bazowy 143),
# sprint zEBA bez broni 692, z bronia 474 (nasz sprint z bronia 600 - plugin gra go we wlasnym tempie, W3SprintArmedSpeed).
LOOPS = {
    "RUN": dict(set=MOV, slow="locomotion_run_cycle_slow_forward", fast="locomotion_run_cycle_fast_forward", speed=345.0, weapon=False),
    "WALK": dict(set=MOV, slow="locomotion_walk_cycle_slow_forward", fast="locomotion_walk_cycle_fast_forward", speed=140.0, weapon=False),
    "SPRINT": dict(set=MOV, slow="locomotion_run_cycle_fast_forward", fast="locomotion_sprint_cycle_forward", speed=692.0, weapon=False),
    "SW_RUN": dict(set=COMBAT, slow="combat_locomotion_run_cycle_slow_forward", fast="combat_locomotion_run_cycle_fast_forward", speed=345.0, weapon=True),
    "SW_WALK": dict(set=COMBAT, slow="combat_locomotion_walk_cycle_slow_forward", fast="combat_locomotion_walk_cycle_fast_forward_override", speed=140.0, weapon=True),
    # sprint z bronia 600 cm/s (decyzja autora 17.10, bylo 474 = tempo zEBA): szybki bieg z mieczem (515 cm/s, krok 344 cm)
    # zmieszany ze sprintem W3 z mieczem (704 cm/s, krok 422 cm, miecz przy biodrze) - mieszanka ok. 0,48, cykl 16 kl.;
    # wolny + szybki bieg siegaly tylko 515 cm/s (wyzej sam szybki bieg w krotszym cyklu = dreptanie)
    "SW_SPRINT": dict(set=COMBAT, slow="combat_locomotion_run_cycle_fast_forward", fast="combat_locomotion_sprint_cycle_forward", speed=600.0, weapon=True),
}
START_FRAMES, STOP_FRAMES = 8, 7      # przejscia jak w HUMANS (T_RUN_2_RUNL 12 kl., T_RUNL_2_RUN 7 kl.)

# nakladka -> [(petla, rodzaj W3, postawa)]; postawa "W3_1H"/"W3_2H" = garda stylu Wiedzmina. Tryb magii (MAGRUN /
# MAGWALK) w nakladkach bez broni: ruch Geralta jak bez broni, przejscia z postawy gotowosci do czaru (S_MAGRUN; S_MAGWALK
# to w HUMANS alias S_MAGRUN). Silnik gra je z NPC_WEAPON_MAG (zgloszenie 21.10: z czarem w reku bieg natywny S_MAGRUNL).
OVERLAYS = {
    "HUMANS_ZDF_W3_LOCO": [("RUN", "RUN", "S_RUN"), ("WALK", "WALK", "S_RUN"),
                           ("MAGRUN", "RUN", "S_MAGRUN"), ("MAGWALK", "WALK", "S_MAGRUN")],
    "HUMANS_ZDF_W3_LOCO_1H": [("1HRUN", "SW_RUN", "W3_1H"), ("1HWALK", "SW_WALK", "W3_1H")],
    "HUMANS_ZDF_W3_LOCO_2H": [("2HRUN", "SW_RUN", "W3_2H"), ("2HWALK", "SW_WALK", "W3_2H")],
    "HUMANS_ZDF_W3_SPRINT": [("RUN", "SPRINT", "S_RUN"), ("MAGRUN", "SPRINT", "S_MAGRUN")],
    "HUMANS_ZDF_W3_SPRINT_1H": [("1HRUN", "SW_SPRINT", "W3_1H")],
    "HUMANS_ZDF_W3_SPRINT_2H": [("2HRUN", "SW_SPRINT", "W3_2H")],
}


def smooth(u):
    u = min(max(u, 0.0), 1.0)
    return u * u * (3 - 2 * u)


def _angle(A, B):
    return math.degrees(math.acos(max(-1.0, min(1.0, (np.trace(A.T @ B) - 1) / 2))))


class Cycle:
    """Jeden cykl chodu: klatki zrodla (fps W3) bez domykajacej, krok (cm), faza 0 = lewa stopa najdalej z przodu."""

    def __init__(self, dump, sk, grip, weapon):
        d = make_moves.to_entity_start(dump)
        fr = retarget.retarget(d, sk, grip, False, "w3", weapon="r_weapon" if weapon else None)
        self.fps = d["fps"]
        n = self._period(fr)
        self.n = n
        self.stride = float(fr[n][1][2] - fr[0][1][2])
        self.T = n / self.fps
        self.quats = [[retarget.quat_from_mat(R) for R in f[0]] for f in fr[:n + 1]]
        # odchylenie korzenia od rownego posuwu (bujanie miednicy), o(n) == o(0)
        self.offset = [np.array(f[1], dtype=float) - np.array([0.0, 0.0, self.stride * i / n]) for i, f in enumerate(fr[:n + 1])]
        foot = sk.idx("BIP01 L FOOT")
        fwd = [sk.compose(f[0], f[1])[foot][2, 3] - f[1][2] for f in fr[:n]]
        self.p0 = int(np.argmax(fwd))

    @staticmethod
    def _period(fr):
        """Dlugosc cyklu: pierwsza klatka (po 1/4 klipu) z poza jak klatka 0 - klip moze miec kilka cykli."""
        last = len(fr) - 1
        d = [max(_angle(a, b) for a, b in zip(fr[0][0], fr[i][0])) for i in range(len(fr))]
        for i in range(max(2, last // 4), last):
            if d[i] < 3.0 and d[i] <= d[i - 1] and d[i] <= d[i + 1]:
                return i
        return last

    def sample(self, u):
        """Faza u (0..1, od lewej stopy z przodu) -> (kwaterniony kosci, odchylenie korzenia)."""
        f = (self.p0 + (u % 1.0) * self.n) % self.n
        a = int(math.floor(f))
        t = f - a
        b = a + 1
        q = [retarget.slerp(x, y, t) for x, y in zip(self.quats[a], self.quats[b])]
        o = self.offset[a] * (1 - t) + self.offset[b] * t
        return q, o


class LoopModel:
    """Mieszanka cyklu wolnego i szybkiego w tempie 'speed' (cm/s) w petli @25 kl./s."""

    def __init__(self, slow_dump, fast_dump, sk, grip, weapon, speed):
        self.a, self.b = Cycle(slow_dump, sk, grip, weapon), Cycle(fast_dump, sk, grip, weapon)
        a, b = self.a, self.b
        den = (b.stride - a.stride) - speed * (b.T - a.T)
        w = (speed * a.T - a.stride) / den if abs(den) > 1e-6 else 0.0
        self.w = min(max(w, 0.0), 1.0)
        T = a.T + (b.T - a.T) * self.w
        stride = a.stride + (b.stride - a.stride) * self.w
        if abs(stride / T - speed) > 1.0:   # poza zakresem mieszanki - tempo przez czas trwania cyklu
            T = stride / speed
        self.N = max(8, int(round(T * retarget.FPS_OUT)))
        self.speed = speed
        self.step = speed / retarget.FPS_OUT            # cm na klatke
        self.stride = self.step * self.N

    def pose(self, u):
        qa, oa = self.a.sample(u)
        qb, ob = self.b.sample(u)
        local = [qmat(*retarget.slerp(x, y, self.w)) for x, y in zip(qa, qb)]
        return local, oa * (1 - self.w) + ob * self.w

    def loop_frames(self):
        out = []
        for k in range(self.N):
            local, o = self.pose(k / self.N)
            out.append((local, np.array([o[0], o[1], o[2] + self.step * k])))
        return out

    def start_frames(self, idle, n):
        """Z postawy do 1. klatki petli: kroki od razu, predkosc rosnie, poza przechodzi z postawy w bieg."""
        v = [self.step * smooth((k + 1) / n) for k in range(n)]
        d = np.concatenate([[0.0], np.cumsum(v)])            # d[k] = droga do klatki k
        end = d[n]                                            # tu wypada 1. klatka petli
        _, o0 = self.pose(0.0)
        out = []
        for k in range(n):
            u = -(end - d[k]) / self.stride
            local, o = self.pose(u)
            w = smooth(k / n)
            q = [qmat(*retarget.slerp(retarget.quat_from_mat(I), retarget.quat_from_mat(L), w)) for I, L in zip(idle[0], local)]
            x = idle[1][0] + (o[0] - idle[1][0]) * w
            y = idle[1][1] + (o[1] - idle[1][1]) * w
            out.append((q, np.array([x, y, o0[2] - (end - d[k])])))
        return out

    def stop_frames(self, idle, n):
        """Z 1. klatki petli do postawy: hamowanie, ostatnia klatka = postawa."""
        v = [self.step * (1 - smooth(k / n)) for k in range(n)]
        d = np.concatenate([[0.0], np.cumsum(v)])
        _, o0 = self.pose(0.0)
        out = []
        for k in range(n):
            local, o = self.pose(d[k] / self.stride)
            w = 1 - smooth(k / (n - 1))
            q = [qmat(*retarget.slerp(retarget.quat_from_mat(I), retarget.quat_from_mat(L), w)) for I, L in zip(idle[0], local)]
            x = idle[1][0] + (o[0] - idle[1][0]) * w
            y = idle[1][1] + (o[1] - idle[1][1]) * w
            out.append((q, np.array([x, y, o0[2] + d[k]])))
        out[-1] = ([R.copy() for R in idle[0]], np.array([idle[1][0], idle[1][1], out[-1][1][2]]))
        return out


def ani_chunk(name, nframes, defs, ground):
    """Definicja jak w HUMANS (warstwa, nastepna, flagi), z miekkim wejsciem - petla W3 nie zaczyna sie poza Gothica."""
    base = defs[name]
    blend_in = max(base["blend_in"], 0.1)
    ani = msblib.ani_chunk(name, base["layer"], base["next"], blend_in, base["blend_out"], base["flags"], "ZDISM_W3.ASC", 1, nframes)
    return ani + sfx_events.events_block(ground=ground)


def idle_pose(vfs, sk, grip, stance):
    """Poza postawy przejsc T_*_2_*L / T_*L_2_*: garda stylu W3 ("W3_1H" / "W3_2H" = garda parady) albo postawa gry."""
    if stance.startswith("W3_"):
        w = stance[3:]
        c = make_attacks.guard_clip(w)
        d = make_pair.dump_clip(make_moves.SWORD_SET, c, os.path.join(make_moves.WORK, c + ".json"))
        return make_attacks.style_guard(d, sk, grip, w)[0]
    return recovery.stance_pose(vfs, sk, stance)


def write_ani(vfs, sk, overlay, name, frames, defs):
    _, ground = sfx_events.motion_sounds(frames, sk)
    # 24.10: ramka bez drogi cyklu (jak petle gry) - inaczej kolizja wypychala bohatera o metry (log NB 24.10, schodki swiatyni)
    nf = make_pair.write_man(vfs, sk, frames, name, os.path.join(COMPILED, "%s-%s.MAN" % (overlay, name)), bbox_inplace=True)
    return ani_chunk(name, nf, defs, ground)


def main(only=None):
    if only:
        bad = [o for o in only if o not in OVERLAYS]
        if bad:
            raise SystemExit("nieznane nakladki: %s (sa: %s)" % (", ".join(bad), ", ".join(OVERLAYS)))
    vfs = build_overlay.game_vfs()
    sk = retarget.GothicSkeleton(vfs)
    grip = make_pair.grip_local(vfs, sk)
    defs = build_overlay.humans_ani_defs(vfs)
    os.makedirs(WORK, exist_ok=True)
    os.makedirs(COMPILED, exist_ok=True)
    dump = lambda s, c: make_pair.dump_clip(s, c, os.path.join(WORK, c + ".json"))
    models = {}
    for kind, spec in LOOPS.items():
        m = LoopModel(dump(spec["set"], spec["slow"]), dump(spec["set"], spec["fast"]), sk, grip, spec["weapon"], spec["speed"])
        models[kind] = m
        print("%-9s mieszanka %.2f, cykl %d kl. @25 (%.2f s), krok %.0f cm, %.0f cm/s" % (
            kind, m.w, m.N, m.N / retarget.FPS_OUT, m.stride, m.speed))
    for overlay, items in OVERLAYS.items():
        if only and overlay not in only:
            continue
        chunks = []
        for base, kind, stance in items:
            m = models[kind]
            idle = idle_pose(vfs, sk, grip, stance)
            loop = "S_%sL" % base
            chunks.append(write_ani(vfs, sk, overlay, loop, m.loop_frames(), defs))
            for name, frames in (("T_%s_2_%sL" % (base, base), m.start_frames(idle, START_FRAMES)),
                                 ("T_%sL_2_%s" % (base, base), m.stop_frames(idle, STOP_FRAMES))):
                if name in defs:
                    chunks.append(write_ani(vfs, sk, overlay, name, frames, defs))
        build_overlay.write_overlay(overlay, COMPILED, chunks, vfs)
        print("%s: %d animacji" % (overlay, len(chunks)))


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", help="tylko te nakladki (po przecinku), np. HUMANS_ZDF_W3_LOCO,HUMANS_ZDF_W3_SPRINT")
    a = ap.parse_args()
    main([o.strip().upper() for o in a.only.split(",") if o.strip()] if a.only else None)
