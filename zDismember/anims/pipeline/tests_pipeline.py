"""Testy pipeline'u animacji zDismember. Uruchom: py -3.12 -I tests_pipeline.py (z katalogu pipeline)."""
import json, math, os, sys, struct, tempfile, traceback
import numpy as np
import zenkit

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import manlib
import retarget
from retarget import GothicSkeleton, qmat, GAME_DATA

WORK = os.path.join(HERE, "..", "work")
VFS = zenkit.Vfs()
for f in ("Anims.vdf", "Anims_Addon.vdf"):
    VFS.mount_disk(GAME_DATA + "\\" + f, clobber=zenkit.VfsOverwriteBehavior.NEWER)
SK = GothicSkeleton(VFS)


def compose_man(m, frame):
    local = [l[:3, :3].copy() for l in SK.local]
    root = SK.root_pos
    for k, ni in enumerate(m.nodes):
        (x, y, z, w), p = m.decode(m.sample(frame, k))
        local[ni] = qmat(-x, -y, -z, w)
        if SK.parent[ni] < 0:
            root = np.array(p)
    return SK.compose(local, root)


def test_man_quaternion_convention_on_game_anim():
    m = manlib.Man(bytes(VFS.find("HUMANS-S_RUN.MAN").data))
    W = compose_man(m, 0)
    y = lambda n: W[SK.idx(n)][1, 3]
    assert y("BIP01 HEAD") > y("BIP01 PELVIS") > y("BIP01 L FOOT")


def make_2h_man(tmp, inplace=False):
    out = os.path.join(tmp, "TEST-T_2HSFINISH.MAN")
    args = [os.path.join(WORK, "Great Sword Slash.json"), out, "--name", "T_2HSFINISH", "--grip", "T_2HATTACKL"]
    if inplace:
        args.append("--inplace")
    sys.argv = ["retarget.py"] + args
    retarget.main()
    return out


def test_retarget_roundtrip_under_1cm():
    with tempfile.TemporaryDirectory() as tmp:
        path = make_2h_man(tmp)
        m = manlib.Man(open(path, "rb").read())
        assert m.frames > 10 and m.checksum == SK.checksum


def test_analyze_finds_cut_in_front_of_hero():
    import analyze_anim
    with tempfile.TemporaryDirectory() as tmp:
        path = make_2h_man(tmp)
        r = analyze_anim.analyze(path, SK)
        assert 0.3 < r["cutTime"] < 1.8, r
        assert r["offset"][2] > 30.0, r


def test_analyze_inplace_offset_is_closer():
    import analyze_anim
    with tempfile.TemporaryDirectory() as tmp:
        a = analyze_anim.analyze(make_2h_man(tmp), SK)
        b = analyze_anim.analyze(make_2h_man(tmp, inplace=True), SK)
        assert b["offset"][2] < a["offset"][2], (a, b)


def test_analyze_at_given_frame_uses_that_frame():
    import analyze_anim
    with tempfile.TemporaryDirectory() as tmp:
        path = make_2h_man(tmp, inplace=True)
        r = analyze_anim.analyze(path, SK, frame=7)
        assert r["cutFrame"] == 7 and abs(r["cutTime"] - 7 / 25.0) < 1e-6, r


def test_w3_retarget_keeps_figure_upright_and_facing_forward():
    src = os.path.join(WORK, "w3", "hero_01_rp.json")
    if not os.path.exists(src):
        print("  (pominiety - brak zrzutu W3)")
        return
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "W3-T_1HSFINISH.MAN")
        saved, sys.argv = sys.argv, ["retarget.py", src, out, "--name", "T_1HSFINISH", "--grip", "T_1HATTACKL", "--source", "w3"]
        try:
            retarget.main()
        finally:
            sys.argv = saved
        m = manlib.Man(open(out, "rb").read())
        W = compose_man(m, 0)
        y = lambda n: W[SK.idx(n)][1, 3]
        assert y("BIP01 HEAD") > y("BIP01 PELVIS") > y("BIP01 L FOOT"), "postac nie stoi"
        # lewy obojczyk po lewej stronie miednicy (Gothic: lewa = -X w ukladzie postaci)
        lw, rw = W[SK.idx("BIP01 L CLAVICLE")][:3, 3], W[SK.idx("BIP01 R CLAVICLE")][:3, 3]
        right_axis = W[SK.idx("BIP01")][:3, :3] @ SK.world[SK.idx("BIP01")][:3, :3].T @ np.array([1.0, 0.0, 0.0])
        assert np.dot(lw - rw, right_axis) < 0, "lewy obojczyk po prawej - zle osie"


def test_w3_blade_follows_source_weapon_bone():
    # miecz w Gothicu (os -X ZS_RIGHTHAND) ma isc wzdluz ostrza z W3 (lokalna +Y kosci r_weapon), nie wg chwytu Gothica
    src = os.path.join(WORK, "w3", "hero_01_rp.json")
    if not os.path.exists(src):
        print("  (pominiety - brak zrzutu W3)")
        return
    dump = json.load(open(src))
    wi = [b["name"] for b in dump["bones"]].index("r_weapon")
    frames = retarget.retarget(dump, SK, np.eye(3), False, "w3", weapon="r_weapon")
    gi = SK.idx("ZS_RIGHTHAND")
    for k in (0, 20, 34, 60):
        local, root = frames[k]
        g = SK.compose(local, root)[gi][:3, :3] @ np.array([-1.0, 0.0, 0.0])
        w = retarget.P_W3 @ np.array(dump["frames"][k][wi])[:3, :3] @ retarget.BLADE_AXIS["w3"]
        assert np.dot(g, w / np.linalg.norm(w)) > 0.999, (k, g, w)


def test_blade_contact_finds_crossing_between_frames():
    # ostrze zamiata poziomo przez punkt miedzy klatkami 5 i 6 (w 0,3 drogi) - wynik w ulamku klatki
    import make_pair
    pts = [np.array([0.0, 100.0, 50.0])] * 10
    blade = []
    for k in range(10):
        x = -150.0 + 100.0 * (k - 5.3) + 150.0   # w klatce 5,3 ostrze przechodzi przez x=0
        blade.append((np.array([x, 100.0, 0.0]), np.array([x, 100.0, 100.0])))
    d, f = make_pair.blade_contact(blade, pts, 3, 8)
    assert d < 1.0 and abs(f - 5.3) < 0.051, (d, f)
    # okno klatek ogranicza wyszukiwanie
    d2, f2 = make_pair.blade_contact(blade, pts, 7, 9)
    assert f2 >= 7 and d2 > 50.0, (d2, f2)


def test_native_style_copy_keeps_animations_under_new_name():
    # natywny styl Gothica pod wlasna nazwa (zEBA przykrywa HUMANS_1HST1 swoim plikiem) - te same animacje i MAN-y
    import make_styles
    with tempfile.TemporaryDirectory() as tmp:
        files = make_styles.copy_native(VFS, "HUMANS_1HST1", "HUMANS_ZDF_NAT_1HST1", tmp)
        names = {os.path.basename(f).upper() for f in files}
        orig = zenkit.ModelScript.load(VFS.find("HUMANS_1HST1.MSB"))
        copy = zenkit.ModelScript.load(os.path.join(tmp, "HUMANS_ZDF_NAT_1HST1.MSB"))
        assert [a.name for a in copy.animations] == [a.name for a in orig.animations]
        for a in copy.animations:
            assert ("HUMANS_ZDF_NAT_1HST1-%s.MAN" % a.name.upper()) in names, a.name
        assert "HUMANS_ZDF_NAT_1HST1.MDH" in names


def test_recovery_tail_ends_in_stance_pose():
    # koniec animacji W3 przechodzi w pierwsza klatke postawy Gothica - silnik przejmuje bez przeskoku
    import recovery
    stance_local, stance_root = recovery.stance_pose(VFS, SK, "S_1HRUN")
    p = _pair()
    if p is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    frames = p["heroInplace"]
    out = recovery.append_recovery(frames, (stance_local, stance_root), 8)
    assert len(out) == len(frames) + 8
    last_local, last_root = out[-1]
    assert all(np.allclose(a, b, atol=1e-5) for a, b in zip(last_local, stance_local))
    assert abs(last_root[1] - stance_root[1]) < 1e-4
    assert abs(last_root[0] - frames[-1][1][0]) < 1e-6 and abs(last_root[2] - frames[-1][1][2]) < 1e-6   # bez przesuwu
    # przejscie stopniowe: zaden krok tulowia nie wiekszy niz polowa calego obrotu
    i = SK.idx("BIP01")
    ang = lambda A, B: np.degrees(np.arccos(np.clip((np.trace(A.T @ B) - 1) / 2, -1, 1)))
    total = ang(frames[-1][0][i], stance_local[i])
    steps = [ang(out[k][0][i], out[k + 1][0][i]) for k in range(len(frames) - 1, len(out) - 1)]
    assert max(steps) <= total / 2 + 1e-3, (steps, total)


def test_foot_contacts_detects_landings():
    # stopa opada na ziemie w kl. 5 i 15 (unosi sie miedzy nimi) - zdarzenia krokow w tych klatkach
    import sfx_events
    ys = [20, 14, 9, 5, 2, 0.5, 0.5, 0.5, 4, 10, 16, 12, 7, 3, 1.5, 0.4, 0.4, 0.4, 0.4]
    assert sfx_events.landings(ys, lift=3.0) == [5, 15], sfx_events.landings(ys, lift=3.0)
    # stopa caly czas na ziemi = brak krokow (drganie ponizej progu)
    assert sfx_events.landings([0.5, 1.0, 0.6, 1.2, 0.5], lift=3.0) == []


def test_body_fall_frame():
    import sfx_events
    pel = [90, 90, 85, 70, 50, 30, 18, 15, 15, 16]
    assert sfx_events.body_fall(pel) == 6
    assert sfx_events.body_fall([90, 88, 91, 89]) is None


def test_sound_events_roundtrip_through_zenkit():
    import sfx_events, build_overlay, msblib
    with tempfile.TemporaryDirectory() as tmp:
        ani = msblib.ani_chunk("T_1HPARADEJUMPB", 1, "S_1HRUN", 0.1, 0.2, "M.", "X.ASC", 1, 30)
        ani += sfx_events.events_block(sfx=[(3, "D_PARADE"), (20, "M_FALL_SMALL")], ground=[(8, "RUN"), (16, "RUN")])
        build_overlay.write_overlay("TEST_SFX", tmp, [ani], VFS)
        a = read_msb_anis(os.path.join(tmp, "TEST_SFX.MSB"))["T_1HPARADEJUMPB"]
        assert [(s.frame, s.name) for s in a.sound_effects] == [(3, "D_PARADE"), (20, "M_FALL_SMALL")]
        assert [(s.frame, s.name) for s in a.sound_effects_ground] == [(8, "RUN"), (16, "RUN")]


def test_attack_events_roundtrip_through_zenkit():
    # zdarzenia ataku (trafienie, koniec, okno kombinacji, swist) musza sie odczytac tak jak w HUMANS_1HST1
    import make_attacks, build_overlay
    with tempfile.TemporaryDirectory() as tmp:
        ani = make_attacks.attack_ani_chunk("T_1HATTACKL", 23, hit=9)
        build_overlay.write_overlay("TEST_ATK", tmp, [ani], VFS)
        a = read_msb_anis(os.path.join(tmp, "TEST_ATK.MSB"))["T_1HATTACKL"]
        tags = {int(e.type): list(e.frames) for e in a.event_tags}
        assert tags.get(20) == [9] and tags.get(21) == [23] and tags.get(22) == [12, 23], tags   # OPT_FRAME, HIT_END, WINDOW
        assert 16 in tags, "brak DEF_HIT_LIMB"
        assert [(s.frame, s.name) for s in a.sound_effects] == [(7, "WHOOSH")]
        assert a.last_frame == 23 and a.next == ""   # jak w oryginale: przejscie wybiera silnik


def test_w3_combo_has_four_hits_and_smooth_joins():
    # kombinacja: 4 segmenty jak w nakladkach talentu (OPT/HIT_END/WINDOW rosnaco, okno w swoim segmencie),
    # bez skokow pozy na sklejeniach: tulow i uda < 25 st., korzen < 25 cm (w srodku ciecia W3 biodra
    # legalnie obracaja sie do ~45 st. na klatke, wiec sprawdzane sa tylko sklejenia)
    import make_attacks, make_pair
    dumps = {}
    for c in make_attacks.COMBO_CANDIDATES:
        d = _move_dump(c)
        if d is None:
            print("  (pominiety - brak zrzutow W3)")
            return
        dumps[c] = d
    frames, ev = make_attacks.build_combo(dumps, SK, make_pair.grip_local(VFS, SK), 0.25, 4)
    opt, end, win = ev["opt"], ev["end"], ev["win"]
    assert len(opt) == 4 and len(end) == 4 and len(win) == 8, ev
    prev_end = 0
    for k in range(4):
        assert prev_end < opt[k] < end[k], (k, ev)
        assert opt[k] < win[2 * k] < win[2 * k + 1] <= end[k], (k, ev)
        prev_end = end[k]
    assert end[-1] == len(frames)
    body = [SK.idx(n) for n in ("BIP01", "BIP01 SPINE", "BIP01 SPINE1", "BIP01 SPINE2", "BIP01 L THIGH", "BIP01 R THIGH")]
    for f in end[:-1]:   # ostatnia klatka segmentu (f - 1) -> pierwsza nastepnego (f), indeksy od 0
        (la, ra), (lb, rb) = frames[f - 1], frames[f]
        assert np.linalg.norm(np.array(rb) - np.array(ra)) < 25.0, ("skok korzenia na sklejeniu", f)
        for i in body:
            c = (np.trace(la[i].T @ lb[i]) - 1.0) / 2.0
            assert np.degrees(np.arccos(np.clip(c, -1.0, 1.0))) < 25.0, ("skok pozy na sklejeniu", f, SK.names[i])


def _move_dump(clip):
    path = os.path.join(WORK, "w3moves", clip + ".json")
    return json.load(open(path)) if os.path.exists(path) else None


def test_moves_dodge_left_goes_left_in_place():
    import make_moves, make_pair
    d = _move_dump(make_moves.DODGES["L"])
    if d is None:
        print("  (pominiety - brak zrzutu W3)")
        return
    frames, traj, iframes, cancel = make_moves.build_move(d, SK, make_pair.grip_local(VFS, SK), 0.6, 0.7)
    x, z = traj[-1]
    assert x < -150.0 and abs(z) < 0.3 * abs(x), ("unik w lewo nie idzie w lewo", x, z)
    assert all(abs(r[0]) < 1e-6 and abs(r[2]) < 1e-6 for _, r in frames), "animacja nie jest w miejscu"
    # cancel = koniec ruchu razem z doklejonym przejsciem do postawy
    assert 0.0 < iframes[0] < iframes[1] <= cancel <= (len(frames) + make_moves.REC_FRAMES) / 25.0 + 1e-6, (iframes, cancel)


def test_moves_roll_concat_is_continuous_and_turns_body():
    import make_moves, make_pair
    dumps = [_move_dump(c) for c in make_moves.ROLL]
    if any(x is None for x in dumps):
        print("  (pominiety - brak zrzutu W3)")
        return
    roll = make_moves.concat(dumps)
    names = [b["name"] for b in roll["bones"]]
    pel = [np.array(f[names.index("pelvis")])[:3, 3] * 100 for f in roll["frames"]]
    k = len(dumps[0]["frames"]) - 1
    step = lambda i: np.linalg.norm(pel[i + 1] - pel[i])
    # przewrot jedzie ~5 m/s (~17 cm na klatke) - na sklejeniu krok jak obok, a nie skok o metry
    assert step(k) < 1.5 * max(step(k - 1), step(k + 1)) and step(k) < 30.0, ("skok na sklejeniu", step(k - 1), step(k), step(k + 1))
    frames, traj, _, _ = make_moves.build_move(roll, SK, make_pair.grip_local(VFS, SK), 0.75, 0.85, make_moves.ROLL_YAW["L"])
    x, z = traj[-1]
    assert x < -200.0 and abs(z) < 0.3 * abs(x), ("przewrot w lewo nie idzie w lewo", x, z)
    # po przewrocie (ostatnia klatka, juz na nogach) cialo patrzy w strone przewrotu; w trakcie salta kat nic nie mowi
    yaw = np.degrees(make_pair.body_yaw(frames[-1][0][0], SK))
    assert -150.0 < yaw < -30.0, ("cialo po przewrocie w lewo nie patrzy w lewo", yaw)


def test_w3_blade_axis_runs_along_knuckles():
    # rekojesc lezy w piesci wzdluz linii knykci (maly -> wskazujacy), ostrze wychodzi od strony palca wskazujacego;
    # os ostrza W3 musi z nia isc (zla os = miecz wisi w dol / 130 st. korekty nadgarstka)
    src = os.path.join(WORK, "w3", "hero_01_rp.json")
    if not os.path.exists(src):
        print("  (pominiety - brak zrzutu W3)")
        return
    d = json.load(open(src))
    names = [b["name"] for b in d["bones"]]
    cos = []
    for fr in d["frames"][::5]:
        P = lambda n: np.array(fr[names.index(n)])
        line = P("r_index1")[:3, 3] - P("r_pinky1")[:3, 3]
        cos.append(float(np.dot(P("r_weapon")[:3, :3] @ retarget.BLADE_AXIS["w3"], line / np.linalg.norm(line))))
    assert np.mean(cos) > 0.6, np.mean(cos)


def test_w3_weapon_slot_keeps_constant_gothic_grip():
    # wezel broni ma staly chwyt jak w animacjach Gothica (obrot tylko przez nadgarstek) - inaczej przerwana animacja
    # zostawia przekrecony miecz w animacjach zEBA, ktore tym wezlem nie steruja
    src = os.path.join(WORK, "w3", "hero_01_rp.json")
    if not os.path.exists(src):
        print("  (pominiety - brak zrzutu W3)")
        return
    grip = SK.local[SK.idx("ZS_RIGHTHAND")][:3, :3]
    frames = retarget.retarget(json.load(open(src)), SK, grip, False, "w3", weapon="r_weapon")
    gi = SK.idx("ZS_RIGHTHAND")
    for k in range(0, len(frames), 7):
        assert np.allclose(frames[k][0][gi], grip, atol=1e-5), k


def test_w3_guard_idle_loops_in_place():
    # postawa W3 (S_1HRUN stylu): poza gardy z 1. klatki ciecia + oddech; petla bez szwu, w miejscu, staly chwyt
    import make_attacks, make_pair
    d = _move_dump(make_attacks.STYLES["1H"]["L"])
    if d is None:
        print("  (pominiety - brak zrzutu W3)")
        return
    frames = make_attacks.guard_loop(d, SK, make_pair.grip_local(VFS, SK), 60)
    assert len(frames) == 60
    ang = lambda A, B: np.degrees(np.arccos(np.clip((np.trace(A.T @ B) - 1) / 2, -1, 1)))
    # ostatnia klatka przechodzi w pierwsza tak samo jak sasiednie klatki miedzy soba (bez szarpniecia na petli)
    step = max(ang(a, b) for a, b in zip(frames[0][0], frames[1][0]))
    wrap = max(ang(a, b) for a, b in zip(frames[-1][0], frames[0][0]))
    assert wrap <= step * 1.5 + 0.05, (wrap, step)
    for local, root in frames:
        assert abs(root[0]) < 1e-6 and abs(root[2]) < 1e-6
    gi = SK.idx("ZS_RIGHTHAND")
    assert all(np.allclose(f[0][gi], frames[0][0][gi], atol=1e-5) for f in frames)


def _ang(A, B):
    return np.degrees(np.arccos(np.clip((np.trace(A.T @ B) - 1) / 2, -1, 1)))


def test_w3_parade_starts_and_ends_in_guard():
    # blok W3 (T_1HPARADE_*): z postawy stylu, odbicie, z powrotem do postawy - bez przeskoku na poczatku i koncu
    import make_attacks, make_pair
    d, p = _move_dump(make_attacks.STYLES["1H"]["L"]), _move_dump(make_attacks.PARADES["1H"][0])
    if d is None or p is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    grip = make_pair.grip_local(VFS, SK)
    guard = make_attacks.guard_loop(d, SK, grip)[0]
    frames = make_attacks.build_parade(p, SK, grip, guard, 0.3)
    assert 12 <= len(frames) <= 30, len(frames)
    assert max(_ang(a, b) for a, b in zip(frames[0][0], guard[0])) < 1.0
    assert all(np.allclose(a, b, atol=1e-6) for a, b in zip(frames[-1][0], guard[0]))


def test_w3_counter_deflects_then_strikes_seamlessly():
    # kontra = odbicie (parry) + riposta (szybkie ciecie): cios po odbiciu, sklejenie bez szarpniecia, staly chwyt
    import make_attacks, make_moves, make_pair
    d = _move_dump(make_attacks.STYLES["1H"]["L"])
    c = make_moves.COUNTERS["COUNTER_L"]
    p, a = _move_dump(c["parry"]), _move_dump(c["attack"])
    if d is None or p is None or a is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    grip = make_pair.grip_local(VFS, SK)
    frames, hit, joins = make_moves.build_counter(p, a, SK, grip)
    assert 10 < hit < len(frames) - 5, (hit, len(frames))
    body = [SK.idx(n) for n in ("BIP01", "BIP01 SPINE", "BIP01 SPINE1", "BIP01 SPINE2", "BIP01 L THIGH", "BIP01 R THIGH")]
    for j in joins:
        assert max(_ang(frames[j - 1][0][i], frames[j][0][i]) for i in body) < 25.0, j
    gi = SK.idx("ZS_RIGHTHAND")
    assert all(np.allclose(f[0][gi], frames[0][0][gi], atol=1e-5) for f in frames)


def _loco_dump(clip):
    path = os.path.join(WORK, "w3loco", clip + ".json")
    return json.load(open(path)) if os.path.exists(path) else None


def _loco_model(kind):
    import make_loco, make_pair
    spec = make_loco.LOOPS[kind]
    a, b = _loco_dump(spec["slow"]), _loco_dump(spec["fast"])
    if a is None or b is None:
        return None, spec
    return make_loco.LoopModel(a, b, SK, make_pair.grip_local(VFS, SK), spec["weapon"], spec["speed"]), spec


def test_loco_run_loop_has_gothic_speed_and_closes_seamlessly():
    # trucht W3 w tempie Gothica (345 cm/s): mieszanie cyklu wolnego i szybkiego, petla bez szwu
    model, spec = _loco_model("RUN")
    if model is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    frames = model.loop_frames()
    n = len(frames)
    z = np.array([f[1][2] for f in frames])
    speed = np.polyfit(np.arange(n), z, 1)[0] * retarget.FPS_OUT
    assert abs(speed - spec["speed"]) / spec["speed"] < 0.03, speed
    steps = [max(_ang(x, y) for x, y in zip(frames[k][0], frames[(k + 1) % n][0])) for k in range(n)]
    assert steps[-1] < 1.5 * max(steps[:-1]), (steps[-1], max(steps[:-1]))


def test_loco_feet_stay_planted_while_blended():
    # stopa w fazie podporu prawie stoi w swiecie (bez slizgu po zmianie tempa)
    for kind in ("RUN", "WALK"):
        model, spec = _loco_model(kind)
        if model is None:
            print("  (pominiety - brak zrzutow W3)")
            return
        frames = model.loop_frames()
        foot = [SK.compose(l, r)[SK.idx("BIP01 L FOOT")][:3, 3] for l, r in frames]
        v = [np.linalg.norm(foot[k + 1] - foot[k]) * retarget.FPS_OUT for k in range(len(foot) - 1)]
        assert min(v) < 0.15 * spec["speed"], (kind, min(v))


def test_loco_transitions_join_loop_and_idle():
    # start: z postawy do 1. klatki petli; stop: z petli do postawy (ostatnia klatka = postawa)
    import recovery
    model, spec = _loco_model("RUN")
    if model is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    idle = recovery.stance_pose(VFS, SK, "S_RUN")
    loop = model.loop_frames()
    start, stop = model.start_frames(idle, 8), model.stop_frames(idle, 7)
    body = [SK.idx(n) for n in ("BIP01", "BIP01 PELVIS", "BIP01 SPINE1", "BIP01 L THIGH", "BIP01 R THIGH", "BIP01 L CALF")]
    assert max(_ang(start[-1][0][i], loop[0][0][i]) for i in body) < 25.0
    assert max(_ang(start[0][0][i], idle[0][i]) for i in body) < 25.0
    assert max(_ang(stop[0][0][i], loop[0][0][i]) for i in body) < 25.0
    assert all(np.allclose(a, b, atol=1e-6) for a, b in zip(stop[-1][0], idle[0]))
    assert start[-1][1][2] < loop[0][1][2] and stop[0][1][2] >= loop[0][1][2] - 1e-6


def _sign_dump(clip):
    path = os.path.join(WORK, "w3signs", clip + ".json")
    return json.load(open(path)) if os.path.exists(path) else None


def test_sign_gesture_is_upper_body_and_casts_forward():
    # gest Znaku: tylko tulow i rece (nogi dalej biegna), dlon w chwili pchniecia przed klatka piersiowa
    import make_signs, make_pair, recovery
    for clip, weapon in ((make_signs.SIGNS["AARD"][1], True), (make_signs.SIGNS["AARD"][0], False)):
        d = _sign_dump(clip)
        if d is None:
            print("  (pominiety - brak zrzutu W3)")
            return
        grip = make_pair.grip_local(VFS, SK)
        idle = recovery.stance_pose(VFS, SK, "S_RUN")
        g = make_signs.build_gesture(d, SK, grip, weapon, idle)
        names = [SK.names[i] for i in g["nodes"]]
        assert "BIP01 SPINE" in names and "BIP01 L HAND" in names and "BIP01 R FOREARM" in names
        assert not any(n in names for n in ("BIP01", "BIP01 PELVIS", "BIP01 L THIGH", "BIP01 R CALF"))
        n = len(g["frames"])
        assert 4 <= g["release"] < n - 3, (g["release"], n)
        local = [R.copy() for R in idle[0]]
        for i in g["nodes"]:
            local[i] = g["frames"][g["release"]][0][i]
        W = SK.compose(local, idle[1])
        hand, chest = W[SK.idx("BIP01 L HAND")][:3, 3], W[SK.idx("BIP01 SPINE2")][:3, 3]
        assert hand[2] - chest[2] > 20.0, (clip, hand[2] - chest[2])


def test_sign_gestures_use_distinct_existing_names():
    # jedna nakladka na stale: 10 gestow pod roznymi nazwami, ktore istnieja w HUMANS (nakladka nie doda nowej nazwy)
    import make_signs, build_overlay
    defs = build_overlay.humans_ani_defs(VFS)
    names = list(make_signs.SIGN_ANIS.values())
    assert len(names) == 2 * len(make_signs.SIGNS) and len(set(names)) == len(names)
    assert all(n in defs for n in names), [n for n in names if n not in defs]


def test_w3_counter_bash_lunges_into_enemy_after_parry():
    # kontra barkiem (repel W3): odbicie, potem wypad do przodu; uderzenie w srodku ruchu, sklejenie bez szarpniecia
    import make_moves, make_pair
    for mid in ("BASH_F", "KICK_F"):
        c = make_moves.COUNTERS[mid]
        p, a = _move_dump(c["parry"]), _move_dump(c["attack"])
        if p is None or a is None:
            print("  (pominiety - brak zrzutow W3)")
            return
        grip = make_pair.grip_local(VFS, SK)
        frames, hit, joins = make_moves.build_counter(p, a, SK, grip, hit=c["hit"], start=c["start"], step=c["step"])
        assert 10 < hit < len(frames) - 5, (mid, hit, len(frames))
        body = [SK.idx(n) for n in ("BIP01", "BIP01 SPINE", "BIP01 SPINE1", "BIP01 L THIGH", "BIP01 R THIGH")]
        for j in joins:
            assert max(_ang(frames[j - 1][0][i], frames[j][0][i]) for i in body) < 25.0, (mid, j)
        assert frames[hit][1][2] - frames[joins[0]][1][2] > 50.0, (mid, frames[hit][1][2] - frames[joins[0]][1][2])


def test_w3_run_attack_timing_read_from_humans():
    # atak w biegu W3 ma cios i dlugosc jak T_xHATTACKMOVE gry (tempo walki Gothica): odczyt z HUMANS.MSB jak w zenkit
    import make_attacks
    s = {a.name: a for a in zenkit.ModelScript.load(VFS.find("HUMANS.MSB")).animations}
    for w in ("1H", "2H"):
        a = s["T_%sATTACKMOVE" % w]
        opt = [list(e.frames) for e in a.event_tags if int(e.type) == 20][0]
        assert make_attacks.humans_attack_timing(VFS, "T_%sATTACKMOVE" % w) == (opt[0], a.last_frame - a.first_frame + 1), w


def test_w3_style_overlays_leave_run_attack_to_plugin():
    # atak w biegu W3 gra plugin calym cialem (RUNATK_* w moves_w3.ini) - nakladki stylu nie podmieniaja T_xHATTACKMOVE
    # (gorna polowa W3 nad biegnacymi nogami Gothica wygladala dziwnie); bez pluginu zostaje atak w biegu gry
    import make_attacks
    for w, st in make_attacks.STYLES.items():
        msb = os.path.join(make_attacks.COMPILED, st["overlay"] + ".MSB")
        if not os.path.exists(msb):
            print("  (pominiety - brak nakladki, uruchom make_attacks.py)")
            return
        anis = read_msb_anis(msb)
        assert "T_%sATTACKMOVE" % w not in anis and "T_%sATTACKL" % w in anis, (w, sorted(anis))
        assert not os.path.exists(os.path.join(make_attacks.COMPILED, "%s-T_%sATTACKMOVE.MAN" % (st["overlay"], w))), w


def _msb_layers(data):
    """{warstwa: [nazwy]} z chunkow ani/aniAlias/aniComb (0xF520/0xF530/0xF570) pliku MSB."""
    out, o = {}, 0
    while o < len(data):
        t, l = struct.unpack_from("<HI", data, o)
        body = data[o + 6:o + 6 + l]
        o += 6 + l
        if t in (0xF520, 0xF530, 0xF570):
            p = body.index(b"\n")
            out.setdefault(struct.unpack_from("<I", body, p + 1)[0], []).append(body[:p].decode("latin1"))
    return out


def _all_msb(node, out):
    for c in node.children:
        if c.is_dir():
            _all_msb(c, out)
        elif c.name.upper().startswith("HUMANS") and c.name.upper().endswith(".MSB"):
            out.append(c)
    return out


def test_sign_layer_is_free_and_not_stopped_by_locomotion():
    # warstwa gestow Znakow: silnik co klatke ruchu gasi warstwe 20 (FadeOutAnisLayerRange(20, 20) w _Forward/_Backward/
    # Turn/Strafe..., StopAnisLayerRange(20, 20) w EV_RemoveWeapon), oCNpc::StandUp (dobycie/schowanie broni) gasi 2..8;
    # w HUMANS i jego nakladkach zajete sa 1 (ruch), 2 (atak w biegu, trafienie, dobycie), 4 (celowanie z luku), 9 (patrzenie),
    # 11 (wskazywanie), 20 (zakrety - cale cialo, gesty dialogowe). Gest ma byc nad zakretami (inaczej zakret w miejscu go zakryje).
    import make_signs
    used = {}
    for f in _all_msb(VFS.root, []):
        for layer, names in _msb_layers(bytes(f.data)).items():
            used.setdefault(layer, []).extend(names)
    assert {1, 2, 20} <= set(used), sorted(used)
    L = make_signs.LAYER
    assert L not in used, (L, used.get(L, [])[:5])
    assert not 2 <= L <= 8 and L != 20, L
    assert L > max(used), (L, max(used))
    assert L <= 256, L    # ponizej progu StopAnisLayerRange z StartAIState/Enable (2..256) - przerwanie postaci gasi gest jak inne


def test_sign_tables_match_zquicksign():
    # zQuickSign.cpp: SIGN_LAYER (sprawdzenie, ze aktywna animacja gestu jest z nakladki) i SIGN_ANIS jak w make_signs.py
    import re, make_signs
    src = open(os.path.join(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"), "zQuickSign.cpp"), encoding="latin1").read()
    m = re.search(r"SIGN_LAYER\s*=\s*(\d+)", src)
    assert m and int(m.group(1)) == make_signs.LAYER, m and m.group(1)
    m = re.search(r"SIGN_ANIS\[SIGN_COUNT\]\[2\]\s*=\s*\{(.*?)\};", src, re.S)
    pairs = re.findall(r'\{\s*"(\w+)",\s*"(\w+)"\s*\}', m.group(1))
    order = ["AARD", "IGNI", "QUEN", "YRDEN", "AXII"]   # enum SIGN_* w zQuickSign.cpp
    assert pairs == [(make_signs.SIGN_ANIS[(s, False)], make_signs.SIGN_ANIS[(s, True)]) for s in order], pairs


def test_sign_overlay_files_use_sign_layer():
    # wygenerowana nakladka (make_signs.py): wszystkie gesty w MSB i w MAN na warstwie LAYER, bez nastepnej animacji
    import make_signs
    msb = os.path.join(make_signs.COMPILED, make_signs.OVERLAY + ".MSB")
    if not os.path.exists(msb):
        print("  (pominiety - brak nakladki, uruchom make_signs.py)")
        return
    anis = read_msb_anis(msb)
    # nakladka ruchu niesie tez klipy konca podtrzymania Igni / Quen (test_sign_stop_clips_leave_hold_pose)
    assert sorted(anis) == sorted(list(make_signs.SIGN_ANIS.values()) + list(make_signs.SIGN_ANIS_STOP.values())), sorted(anis)
    for name, a in anis.items():
        m = manlib.Man(open(os.path.join(make_signs.COMPILED, "%s-%s.MAN" % (make_signs.OVERLAY, name)), "rb").read())
        assert a.layer == make_signs.LAYER and m.layer == make_signs.LAYER and a.next == "", (name, a.layer, m.layer, a.next)
        assert m.frames == a.last_frame - a.first_frame + 1, name


def _world_speeds(frames, names):
    """Predkosci katowe kosci w ukladzie modelu: [klatka][kosc] -> wektor (st./kl.); korzen: przesuniecia na klatke."""
    import make_moves
    ids = [SK.idx(n) for n in names]
    W = [[M[:3, :3] for M in SK.compose(l, np.zeros(3))] for l, _ in frames]
    vel = [[np.degrees(make_moves.rotvec(W[k][i].T @ W[k + 1][i])) for i in ids] for k in range(len(frames) - 1)]
    rv = [np.array(frames[k + 1][1], dtype=float) - np.array(frames[k][1], dtype=float) for k in range(len(frames) - 1)]
    return vel, rv


def _motion_stats(frames, names):
    """-> (max predkosc kosci na klatke, max zmiana predkosci kosci, zmiana predkosci korzenia) dla kolejnych klatek."""
    vel, rv = _world_speeds(frames, names)
    speed = [max(np.linalg.norm(v) for v in vk) for vk in vel]
    acc = [max(np.linalg.norm(vel[k + 1][j] - vel[k][j]) for j in range(len(names))) for k in range(len(vel) - 1)]
    racc = [float(np.linalg.norm(rv[k + 1] - rv[k])) for k in range(len(rv) - 1)]
    return speed, acc, racc


def test_w3_counter_joins_are_inertial_and_smooth():
    # kontra z gardy W3: wejscie w odbicie, odbicie -> riposta / repel, koniec -> postawa Gothica. Na sklejeniach bez
    # przeskokow (kosc nie obraca sie o > 135 st. na klatke - tak wygladal przeskok slerpa przez 180 st. w COUNTER_R
    # i KICK_F), przyspieszenie kosci i korzenia nie wieksze niz w samych klipach W3 (stary slerp od zamrozonej pozy:
    # 2x wiecej), pierwsza klatka = garda, ostatnia = postawa, staly chwyt broni
    import make_attacks, make_moves, make_pair, recovery
    g = _move_dump(make_attacks.STYLES["1H"]["L"])
    if g is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    grip = make_pair.grip_local(VFS, SK)
    guard = make_attacks.guard_loop(g, SK, grip)[0]
    stance = recovery.stance_pose(VFS, SK, "S_1HRUN")
    names = make_moves.JOIN_BONES
    gi = SK.idx("ZS_RIGHTHAND")
    for mid, c in make_moves.COUNTERS.items():
        p, a = _move_dump(c["parry"]), _move_dump(c["attack"])
        if p is None or a is None:
            print("  (pominiety - brak zrzutow W3)")
            return
        m, hit, joins = make_moves.build_counter(p, a, SK, grip, c.get("hit"), c.get("start"), c.get("step"), entry=guard)
        full = make_moves.inertial_recovery(m, stance, make_moves.REC_FRAMES)
        pf, _ = make_attacks.prep_attack(p, SK, grip)
        af, _ = make_attacks.prep_attack(a, SK, grip)
        _, acc_p, racc_p = _motion_stats(pf, names)
        _, acc_a, racc_a = _motion_stats(af, names)
        src_acc, src_racc = max(acc_p + acc_a), max(racc_p + racc_a)
        # przed kontra bohater stoi w gardzie (klatka 0 powtorzona) - ruszenie z miejsca tez jest sklejeniem
        speed, acc, racc = _motion_stats([full[0]] + full, names)
        assert max(speed) < 135.0, (mid, int(np.argmax(speed)), max(speed))
        for j in [1] + [x + 1 for x in joins] + [len(m) + 1]:   # garda -> odbicie -> cios -> postawa
            win = range(max(0, j - 2), min(len(acc), j + max(make_moves.COUNTER_BLEND, make_moves.COUNTER_ENTRY)))
            assert max(acc[k] for k in win) <= 1.1 * src_acc, (mid, j, max(acc[k] for k in win), src_acc)
            assert max(racc[k] for k in win) <= 1.1 * src_racc + 0.5, (mid, j, max(racc[k] for k in win), src_racc)
        assert all(np.allclose(x, y, atol=1e-6) for x, y in zip(full[0][0], guard[0])), mid
        assert all(np.allclose(x, y, atol=1e-6) for x, y in zip(full[-1][0], stance[0])), mid
        assert all(np.allclose(f[0][gi], grip, atol=1e-5) for f in full), mid
        assert 10 < hit < len(m) - 5, (mid, hit, len(m))


def test_inertial_append_keeps_pose_and_velocity_continuous():
    # syntetycznie: obrot kosci ze stala predkoscia 10 st./kl. przechodzi w klip zaczynajacy sie 90 st. dalej i stojacy;
    # pierwsza doklejona klatka kontynuuje ruch (ok. +10 st.), po n klatkach poza = klip, przejscie bez skoku > 2x
    import make_moves
    n_b = len(SK.names)
    def pose(deg):
        local = [np.eye(3) for _ in range(n_b)]
        local[0] = make_moves.expmap(np.array([0.0, np.radians(deg), 0.0]))
        return (local, np.array([0.0, 90.0, 0.0]))
    a = [pose(10.0 * k) for k in range(5)]                     # 0..40 st.
    b = [pose(130.0) for _ in range(12)]                      # klip stoi w 130 st.
    out = make_moves.inertial_append(a, b, 0, 11, 1.0, 6)
    yaw = [np.degrees(make_moves.rotvec(f[0][0])[1]) for f in out]
    assert abs(yaw[5] - 40.0 - 10.0) < 6.0, yaw[:8]            # dalej ten sam kierunek i tempo
    assert abs(yaw[4 + 6] - 130.0) < 1e-6 and abs(yaw[-1] - 130.0) < 1e-6, yaw
    steps = [yaw[k + 1] - yaw[k] for k in range(len(yaw) - 1)]
    assert all(s >= -1e-6 for s in steps), steps              # bez cofania (przestrzelenia ponad 20 st.)
    assert max(steps) < 2.6 * 15.0, steps                      # 90 st. w 6 klatek: srednio 15 st./kl.


def test_moves_w3_variant_ends_in_w3_guard():
    # wariant _W3 (bohater w stylu W3): unik konczy sie w gardzie W3 - po ruchu bez przeskoku garda Gothica -> W3
    # (srednio 41 st., korzen -11 cm); zwykly wariant dalej w postawie Gothica
    import make_moves, make_attacks, make_pair, recovery
    d, g = _move_dump(make_moves.DODGES["L"]), _move_dump(make_attacks.STYLES["1H"]["L"])
    if d is None or g is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    grip = make_pair.grip_local(VFS, SK)
    guard = make_attacks.guard_loop(g, SK, grip)[0]
    frames, motion, traj, iframes, cancel = make_moves.build_move_full(d, SK, grip, 1.0, 0.5)
    with tempfile.TemporaryDirectory() as tmp:
        saved, make_moves.COMPILED = make_moves.COMPILED, tmp
        try:
            _, lines = make_moves.write_move(VFS, SK, "TEST_L", frames, motion, traj, iframes, cancel, "test", "DODGE",
                                             w3_targets={"T_1HPARADEJUMPB": guard})
        finally:
            make_moves.COMPILED = saved
        assert "overlayW3=HUMANS_ZDF_MV_TEST_L_W3" in lines, lines
        names = ("BIP01", "BIP01 SPINE2", "BIP01 R HAND", "BIP01 L HAND", "BIP01 L FOOT", "BIP01 HEAD")

        def last_pose(ovl):
            m = manlib.Man(open(os.path.join(tmp, "%s-T_1HPARADEJUMPB.MAN" % ovl), "rb").read())
            W = compose_man(m, m.frames - 1)
            return {n: W[SK.idx(n)][:3, 3] for n in names}

        def pose_of(local, root):
            W = SK.compose(local, np.array([0.0, root[1], 0.0]))
            return {n: W[SK.idx(n)][:3, 3] for n in names}

        w3, base = last_pose("HUMANS_ZDF_MV_TEST_L_W3"), last_pose("HUMANS_ZDF_MV_TEST_L")
        want_w3, want_base = pose_of(*guard), pose_of(*recovery.stance_pose(VFS, SK, "S_1HRUN"))
        for n in names:
            assert np.linalg.norm(w3[n] - want_w3[n]) < 1.5, (n, w3[n], want_w3[n])
            assert np.linalg.norm(base[n] - want_base[n]) < 1.5, (n, base[n], want_base[n])


def test_counter_reach_matches_contact_part():
    # zasieg do celowania w grze: ostrze siega dalej niz stopa, stopa dalej niz bark
    import make_moves, make_pair
    grip = make_pair.grip_local(VFS, SK)
    reach = {}
    for mid, c in make_moves.COUNTERS.items():
        p, a = _move_dump(c["parry"]), _move_dump(c["attack"])
        if p is None or a is None:
            print("  (pominiety - brak zrzutow W3)")
            return
        m, hit, _ = make_moves.build_counter(p, a, SK, grip, c.get("hit"), c.get("start"), c.get("step"))
        reach[mid] = make_moves.counter_reach(SK, m, hit, c["contact"])
    assert 20.0 < reach["BASH_F"] < 70.0, reach
    assert 60.0 < reach["KICK_F"] < 130.0, reach
    for mid in ("COUNTER_L", "COUNTER_R"):
        assert reach["KICK_F"] - 20.0 < reach[mid] < 160.0, reach


_THRUST = {}


def _thrust_pair(clip="man_finisher_05_rp", window=(16, 22)):
    """Pchniecie W3 (W3_05/06) i jego wariant egzekucji oszolomionego (ofiara wstaje z kleczenia); None bez zrzutow."""
    if clip not in _THRUST:
        hero = os.path.join(WORK, "w3", "hero_%s.json" % clip)
        victim = os.path.join(WORK, "w3", "victim_%s.json" % clip)
        if not (os.path.exists(hero) and os.path.exists(victim)):
            return None
        import make_pair
        p = make_pair.build_pair(json.load(open(hero)), json.load(open(victim)), SK, VFS, "chest", window)
        _THRUST[clip] = (p, make_pair.rise_pair(p, VFS, SK, "chest"))
    return _THRUST[clip]


_BODY = ["BIP01 HEAD", "BIP01 NECK", "BIP01 SPINE2", "BIP01 SPINE1", "BIP01 PELVIS", "BIP01 L UPPERARM", "BIP01 R UPPERARM",
         "BIP01 L FOREARM", "BIP01 R FOREARM", "BIP01 L HAND", "BIP01 R HAND", "BIP01 L THIGH", "BIP01 R THIGH"]


def test_kneel_rise_goes_from_pray_pose_to_standing():
    # wstawanie = T_PRAY_2_STAND z gry (T_STAND_2_PRAY od konca): start dokladnie w pozie S_PRAY oszolomionego
    import make_pair, recovery
    rise = make_pair.kneel_rise(VFS, SK, 1.4)
    assert 22 <= len(rise) <= 27, len(rise)   # 1,4 s animacji gry x1,4 = ok. 1 s
    pray_local, pray_root = recovery.stance_pose(VFS, SK, "S_PRAY")
    assert np.allclose(rise[0][1], pray_root, atol=0.5), (rise[0][1], pray_root)
    for a, b in zip(rise[0][0], pray_local):
        assert np.allclose(a, b, atol=1e-3)
    assert rise[0][1][1] < 50.0 < 95.0 < rise[-1][1][1], (rise[0][1][1], rise[-1][1][1])


def test_rise_pair_keeps_w3_thrust_after_entry():
    # egzekucja oszolomionego pchnieciem: od wejscia bohatera w zamach W3 obie postacie graja W3 (przesuniete w czasie),
    # wiec ostrze trafia w klatke piersiowa tak samo jak w W3
    r = _thrust_pair()
    if r is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    p, q = r
    s = q["riseShift"]
    assert q["cutFrame"] == p["cutFrame"] + s and abs(q["cutDist"] - p["cutDist"]) < 0.5, (q["cutFrame"], q["cutDist"])
    assert q["cutDist"] < 15.0
    assert len(q["hero"]) == len(p["hero"]) + s and len(q["victim"]) == len(p["victim"]) + s
    assert len(q["heroTraj"]) == len(q["hero"]) and len(q["victimTraj"]) == len(q["victim"])
    assert len(q["victimA"]) + len(q["victimB"]) - 1 == len(q["victim"])
    for t in (q["entryFrame"] + s, q["cutFrame"], len(q["hero"]) - 1):
        assert np.allclose(q["hero"][t][1], p["hero"][t - s][1]) and np.allclose(q["victim"][t][1], p["victim"][t - s][1]), t
        assert all(np.allclose(a, b) for a, b in zip(q["hero"][t][0], p["hero"][t - s][0])), t
    # zamach W3 (sztych z tylu) przed pchnieciem, ofiara wstaje przed wejsciem bohatera
    assert q["entryFrame"] < p["cutFrame"] - 3 and q["riseFrames"] <= q["entryFrame"] + s


def test_rise_pair_victim_starts_kneeling_facing_hero():
    # ofiara zaczyna tam, gdzie kleczy oszolomiony (poza S_PRAY w ukladzie voba, twarza do bohatera), przed bohaterem
    import make_pair, recovery
    r = _thrust_pair()
    if r is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    p, q = r
    pray_local, pray_root = recovery.stance_pose(VFS, SK, "S_PRAY")
    local, root = q["victimA"][0]
    assert abs(root[0]) < 1e-6 and abs(root[2]) < 1e-6 and abs(root[1] - pray_root[1]) < 0.5, root
    assert all(np.allclose(a, b, atol=1e-3) for a, b in zip(local, pray_local))
    vx, vz = q["victimTraj"][0]
    assert 120.0 < vz < 220.0 and abs(vx) < 0.25 * vz, (vx, vz)
    assert abs(make_pair.body_yaw(local[0], SK)) < np.radians(10)


def test_rise_pair_blade_clear_of_rising_victim():
    # w czasie wstawania bohater stoi w postawie walki z gry (w gardzie W3 sztych jest przy gardle ofiary) - ostrze
    # i ciala nie przenikaja ofiary az do wejscia w zamach W3, dla 1H i 2H
    import make_pair
    for clip, window in (("man_finisher_05_rp", (16, 22)), ("man_finisher_06_lp", (16, 21))):
        r = _thrust_pair(clip, window)
        if r is None:
            print("  (pominiety - brak zrzutow W3)")
            return
        p, q = r
        upto = q["entryFrame"] + q["riseShift"]
        vw = [SK.compose(l, rt) for l, rt in q["victim"][:upto]]
        for stance in ("S_1HRUN", "S_2HRUN"):
            hero = [(l, np.array([x, rt[1], z])) for (l, rt), (x, z) in zip(q["heroInplaceBy"][stance], q["heroTraj"])]
            blade = make_pair.blade_segments(hero[:upto], SK)
            hw = [SK.compose(l, rt) for l, rt in hero[:upto]]
            for k in range(upto):
                for nm in _BODY:
                    pt = vw[k][SK.idx(nm)][:3, 3]
                    d = make_pair.seg_dist(pt, *blade[k])
                    assert d > 15.0, (clip, stance, k, nm, d)
                    assert min(np.linalg.norm(hw[k][SK.idx(h)][:3, 3] - pt) for h in _BODY) > 30.0, (clip, stance, k, nm)


def test_rise_pair_hero_starts_in_weapon_stance():
    # bohater zaczyna w postawie walki swojej broni (stad wchodzi w egzekucje), od zamachu W3 obie bronie tak samo
    import recovery
    r = _thrust_pair()
    if r is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    p, q = r
    by = q["heroInplaceBy"]
    for stance in ("S_1HRUN", "S_2HRUN"):
        local, root = by[stance][0]
        want_local, want_root = recovery.stance_pose(VFS, SK, stance)
        assert abs(root[1] - want_root[1]) < 0.5 and all(np.allclose(a, b, atol=1e-3) for a, b in zip(local, want_local)), stance
        assert len(by[stance]) == len(q["hero"])
    t = q["entryFrame"] + q["riseShift"]
    assert all(np.allclose(a, b) for a, b in zip(by["S_1HRUN"][t][0], by["S_2HRUN"][t][0]))
    assert q["heroTraj"][0] == (0.0, 0.0)


def _sign_pose(m, frame):
    """(lokalne obroty, korzen) dowolnej klatki MAN - jak recovery.stance_pose."""
    local = [l[:3, :3].copy() for l in SK.local]
    root = np.array(SK.root_pos, dtype=float)
    for k, ni in enumerate(m.nodes):
        (x, y, z, w), p = m.decode(m.sample(frame, k))
        local[ni] = qmat(-x, -y, -z, w)
        if SK.parent[ni] < 0:
            root = np.array(p, dtype=float)
    return local, root


_SIGN_VARIANTS = {}


def _sign_variants():
    """{(znak, z mieczem): (wariant postoju, wariant ruchu)} z make_signs.build_gesture; None bez zrzutow W3."""
    import make_signs, make_pair
    if not _SIGN_VARIANTS:
        grip = make_pair.grip_local(VFS, SK)
        run = make_signs.mean_pose(VFS, SK)
        floor = make_signs.run_floor(VFS, SK)
        for sign, (bare, sword) in make_signs.SIGNS.items():
            for clip, weapon in ((sword, True), (bare, False)):
                d = _sign_dump(clip)
                if d is None:
                    _SIGN_VARIANTS.clear()
                    return None
                _SIGN_VARIANTS[(sign, weapon)] = (make_signs.build_gesture(d, SK, grip, weapon, None, True),
                                                  make_signs.build_gesture(d, SK, grip, weapon, run,
                                                                           ground=sign in make_signs.GROUND_SIGNS, floor=floor))
    return _SIGN_VARIANTS


def _sign_push(g, base):
    """Gest nalozony na poze base (warstwa pod gestem) w klatce najdalszego wysuniecia dloni od chwili pchniecia:
    -> (dlon przed klatka piersiowa w cm, odchylenie ramienia (bark -> dlon) od kierunku postaci w st., w poziomie)."""
    hand, chest, arm = SK.idx("BIP01 L HAND"), SK.idx("BIP01 SPINE2"), SK.idx("BIP01 L UPPERARM")
    full = SK.idx("BIP01") in g["nodes"]
    best = None
    for k in range(g["release"], len(g["frames"])):
        local = [R.copy() for R in base[0]]
        for i in g["nodes"]:
            local[i] = g["frames"][k][0][i]
        W = SK.compose(local, g["frames"][k][1] if full else base[1])
        reach = W[hand][2, 3] - W[chest][2, 3]
        a = W[hand][:3, 3] - W[arm][:3, 3]
        if best is None or reach > best[0]:
            best = (reach, abs(float(np.degrees(np.arctan2(a[0], a[2])))))
    return best


def test_sign_stand_gesture_is_full_body_in_place_with_planted_feet():
    # wariant postoju: caly Geralt (korzen, miednica, nogi, tulow, rece, miecz w dloni), korzen bez ruchu w poziomie, stopy
    # stoja (W3 robi wypad o 30-90 cm - po samym wyzerowaniu korzenia stopy slizgalyby sie o 30-55 cm); te same klatki
    # i chwila pchniecia co wariant ruchu (zQuickSign przelacza w trakcie gestu od tej samej klatki)
    v = _sign_variants()
    if v is None:
        print("  (pominiety - brak zrzutu W3)")
        return
    feet = (SK.idx("BIP01 L FOOT"), SK.idx("BIP01 R FOOT"))
    must = {"BIP01", "BIP01 PELVIS", "BIP01 L THIGH", "BIP01 R CALF", "BIP01 L FOOT", "BIP01 SPINE", "BIP01 L HAND", "ZS_RIGHTHAND"}
    for key, (st, mv) in v.items():
        names = {SK.names[i] for i in st["nodes"]}
        assert must <= names and not any(n.startswith("ZS_") and n != "ZS_RIGHTHAND" for n in names), key
        assert len(st["frames"]) == len(mv["frames"]) and st["release"] == mv["release"], key
        assert all(abs(r[0]) < 1e-6 and abs(r[2]) < 1e-6 for _, r in st["frames"]), key
        W0 = SK.compose(*st["frames"][0])
        slide = max(np.linalg.norm(SK.compose(*f)[i][:3, 3] - W0[i][:3, 3]) for f in st["frames"] for i in feet)
        assert slide < 0.5, (key, slide)


def test_sign_stand_gesture_casts_forward_from_any_stance():
    # na postoju dlon idzie do przodu niezaleznie od postawy pod gestem: garda W3 1H/2H (miednica bokiem 77/-54 st. - stad
    # zgloszenie "Znak w prawo zamiast do przodu"), natywne S_1HRUN i S_RUN. Dla porownania wariant ruchu (tulow wzgledem
    # miednicy biegu) na gardzie W3 1H odchyla ramie o ponad 45 st. - dlatego na postoju gra caly gest
    import make_attacks, make_pair, recovery
    v = _sign_variants()
    g1, g2 = _guard("1H", 4), _guard("2H")   # garda 2H = garda parady (parry_idle_lp)
    if v is None or g1 is None or g2 is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    bases = {"W3_1H": g1[0], "W3_2H": g2[0],
             "S_1HRUN": recovery.stance_pose(VFS, SK, "S_1HRUN"), "S_RUN": recovery.stance_pose(VFS, SK, "S_RUN")}
    old = []
    for key, (st, mv) in v.items():
        for b, pose in bases.items():
            reach, dev = _sign_push(st, pose)
            assert reach > 40.0 and dev < 35.0, (key, b, reach, dev)
        old.append(_sign_push(mv, bases["W3_1H"])[1])
    assert max(old) > 45.0, old


def test_sign_move_gesture_casts_forward_over_run_pelvis():
    # w ruchu tylko tulow i rece (nogi biegna): tulow wzgledem sredniej miednicy S_RUNL - dlon do przodu w kazdej klatce
    # biegu natywnego (S_RUNL / S_1HRUNL / S_2HRUNL) i petli biegu stylu W3 (gdy wygenerowane przez make_loco.py)
    import make_signs
    v = _sign_variants()
    if v is None:
        print("  (pominiety - brak zrzutu W3)")
        return
    runs = [("HUMANS-" + n, manlib.Man(bytes(VFS.find("HUMANS-%s.MAN" % n).data))) for n in ("S_RUNL", "S_1HRUNL", "S_2HRUNL")]
    for f in ("HUMANS_ZDF_W3_LOCO-S_RUNL.MAN", "HUMANS_ZDF_W3_LOCO_1H-S_1HRUNL.MAN"):
        p = os.path.join(make_signs.COMPILED, f)
        if os.path.exists(p):
            runs.append((f, manlib.Man(open(p, "rb").read())))
    for key, (st, mv) in v.items():
        assert not {SK.names[i] for i in mv["nodes"]} & {"BIP01", "BIP01 PELVIS", "BIP01 L THIGH", "BIP01 R CALF"}, key
        for name, m in runs:
            for fr in range(0, m.frames, 3):
                reach, dev = _sign_push(mv, _sign_pose(m, fr))
                assert reach > 40.0 and dev < 40.0, (key, name, fr, reach, dev)


def test_sign_variant_tables_match_zquicksign():
    # 20 roznych nazw z HUMANS (nakladka nie doda nowej); tablica postoju i nazwy obu nakladek jak w zQuickSign.cpp
    import re, make_signs, build_overlay
    defs = build_overlay.humans_ani_defs(VFS)
    names = list(make_signs.SIGN_ANIS.values()) + list(make_signs.SIGN_ANIS_STAND.values())
    assert len(names) == 4 * len(make_signs.SIGNS) and len(set(names)) == len(names), names
    assert all(n in defs for n in names), [n for n in names if n not in defs]
    src = open(os.path.join(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"), "zQuickSign.cpp"), encoding="latin1").read()
    m = re.search(r"SIGN_ANIS_STAND\[SIGN_COUNT\]\[2\]\s*=\s*\{(.*?)\};", src, re.S)
    assert m, "brak SIGN_ANIS_STAND w zQuickSign.cpp"
    pairs = re.findall(r'\{\s*"(\w+)",\s*"(\w+)"\s*\}', m.group(1))
    order = ["AARD", "IGNI", "QUEN", "YRDEN", "AXII"]   # enum SIGN_* w zQuickSign.cpp
    assert pairs == [(make_signs.SIGN_ANIS_STAND[(s, False)], make_signs.SIGN_ANIS_STAND[(s, True)]) for s in order], pairs
    for var, val in (("SIGN_OVERLAY", make_signs.OVERLAY), ("SIGN_OVERLAY_STAND", make_signs.OVERLAY_STAND)):
        m = re.search(r'\b%s\s*=\s*"(\w+)"' % var, src)
        assert m and m.group(1) == val, (var, m and m.group(1))


def test_sign_stand_overlay_files_are_full_body_like_move_variant():
    # wygenerowana nakladka postoju (make_signs.py): nazwy z SIGN_ANIS_STAND, warstwa LAYER, bez nastepnej, MAN z korzeniem,
    # miednica i nogami, tyle klatek co gest ruchu tego samego Znaku (przelaczenie od tej samej klatki), wygaszenie
    # BLEND_OUT_STAND
    import make_signs
    man = lambda ovl, n: manlib.Man(open(os.path.join(make_signs.COMPILED, "%s-%s.MAN" % (ovl, n)), "rb").read())
    msb = os.path.join(make_signs.COMPILED, make_signs.OVERLAY_STAND + ".MSB")
    if not os.path.exists(msb):
        print("  (pominiety - brak nakladki, uruchom make_signs.py)")
        return
    anis = read_msb_anis(msb)
    # nakladka postoju niesie tez gesty trybu alternatywnego "w ziemie" (test_sign_alt_overlay_files_in_stand_overlay)
    assert sorted(anis) == sorted(list(make_signs.SIGN_ANIS_STAND.values()) + list(make_signs.SIGN_ANIS_ALT.values())), sorted(anis)
    for key, name in make_signs.SIGN_ANIS_STAND.items():
        a, m, mv = anis[name], man(make_signs.OVERLAY_STAND, name), man(make_signs.OVERLAY, make_signs.SIGN_ANIS[key])
        assert a.layer == make_signs.LAYER and m.layer == make_signs.LAYER and a.next == "", (name, a.layer, m.layer, a.next)
        assert m.frames == a.last_frame - a.first_frame + 1 == mv.frames, (name, m.frames, mv.frames)
        assert abs(a.blend_out - make_signs.BLEND_OUT_STAND) < 1e-5, (name, a.blend_out)
        assert {"BIP01", "BIP01 PELVIS", "BIP01 L THIGH", "BIP01 R FOOT"} <= {SK.names[i] for i in m.nodes}, name


def test_sign_stop_clips_leave_hold_pose():
    # koniec strumienia Igni / tarczy Quen (23.10): klip W3 *_stop w wariancie ruchu (tylko tulow i rece, nogi biegna)
    # zaczyna sie blisko pozy trzymania (gest zwykly w klatce SignChannelFrame=10 nad biegiem) i opuszcza reke
    import make_signs, make_pair
    grip = make_pair.grip_local(VFS, SK)
    run = make_signs.mean_pose(VFS, SK)
    hand = SK.idx("BIP01 L HAND")
    assert set(make_signs.STOP_SIGNS) == {"IGNI", "QUEN"}
    for sign, (bare, sword) in make_signs.STOP_SIGNS.items():
        for clip, weapon in ((bare, False), (sword, True)):
            d, dg = _sign_dump(clip), _sign_dump(make_signs.SIGNS[sign][1 if weapon else 0])
            if d is None or dg is None:
                print("  (pominiety - brak zrzutu W3)")
                return
            st = make_signs.build_stop_gesture(d, SK, grip, weapon, run)
            names = {SK.names[i] for i in st["nodes"]}
            assert "BIP01 SPINE" in names and not names & {"BIP01", "BIP01 PELVIS", "BIP01 L THIGH"}, (sign, weapon)
            assert 8 <= len(st["frames"]) <= 22, (sign, weapon, len(st["frames"]))
            hold = make_signs.build_gesture(dg, SK, grip, weapon, run)["frames"][10]
            pose = lambda f: SK.compose(make_signs.ground_pose(SK, f, run), run[1])
            W0, Wh, We = pose(st["frames"][0]), pose(hold), pose(st["frames"][-1])
            gap = np.linalg.norm(W0[hand][:3, 3] - Wh[hand][:3, 3])
            assert gap < 20.0, (sign, weapon, gap)                         # start przy pozie trzymania
            drop = W0[hand][1, 3] - We[hand][1, 3]
            assert drop > 15.0, (sign, weapon, drop)                       # reka opuszczona


def test_sign_stop_tables_match_zdismember():
    # 4 nazwy klipow konca (gesty strazy G1 i taniec ZS_Dance - tylko NPC) w HUMANS, inne niz pozostale gesty; te same
    # w Signs.cpp (SgChannelStopName: Igni / Quen, z mieczem / bez)
    import re, make_signs, build_overlay
    defs = build_overlay.humans_ani_defs(VFS)
    stop = list(make_signs.SIGN_ANIS_STOP.values())
    other = (list(make_signs.SIGN_ANIS.values()) + list(make_signs.SIGN_ANIS_STAND.values()) +
             list(make_signs.SIGN_ANIS_ALT.values()))
    assert len(set(stop)) == 4 and not set(stop) & set(other), stop
    assert all(n in defs for n in stop), [n for n in stop if n not in defs]
    src = open(os.path.join(HERE, "..", "..", "zDismember", "Signs.cpp"), encoding="latin1").read()
    m = re.search(r"SgChannelStopName\(int sign, bool sword\) \{(.*?)\n\t\}", src, re.S)
    assert m, "brak SgChannelStopName w Signs.cpp"
    pairs = re.findall(r'sword \? "(\w+)" : "(\w+)"', m.group(1))
    assert pairs == [(make_signs.SIGN_ANIS_STOP[("IGNI", True)], make_signs.SIGN_ANIS_STOP[("IGNI", False)]),
                     (make_signs.SIGN_ANIS_STOP[("QUEN", True)], make_signs.SIGN_ANIS_STOP[("QUEN", False)])], pairs


def test_sign_push_end_follows_release_and_matches_quicksign():
    # tryb zwykly (23.10, zQuickSign SignCastSync): skutek z koncem pchniecia dloni - po najszybszym ruchu, w gescie, ten
    # sam w obu wariantach (postoj / ruch); domyslne SignCastSyncMs w zQuickSign.cpp = najpozniejszy koniec z wariantow
    import re, make_signs
    v = _sign_variants()
    if v is None:
        print("  (pominiety - brak zrzutu W3)")
        return
    ends = {}
    for (sign, weapon), (st, mv) in v.items():
        assert st["push_end"] == mv["push_end"], (sign, weapon)
        assert st["release"] <= st["push_end"] < len(st["frames"]), (sign, weapon, st["release"], st["push_end"])
        ends[sign] = max(ends.get(sign, 0), st["push_end"])
    want = ",".join(str(round(ends[s] / retarget.FPS_OUT * 1000)) for s in make_signs.QS_ORDER)
    src = open(os.path.join(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"), "zQuickSign.cpp"), encoding="latin1").read()
    m = re.search(r'signCastSyncMsText\[\d+\]\s*=\s*"([\d,]+)"', src)
    m2 = re.search(r"int syncMs\[SIGN_COUNT\]\s*=\s*\{([\d, ]+)\}", src)
    assert m and m.group(1) == want, (m and m.group(1), want)
    assert m2 and m2.group(1).replace(" ", "") == want, (m2 and m2.group(1), want)
    # skutek nie wczesniej niz dotad przy pchnieciu (SignCastMs 320) i nie pozniej niz 0,5 s po puszczeniu F
    assert all(280 <= round(ends[s] / retarget.FPS_OUT * 1000) <= 500 for s in ends), ends


_SIGN_ALT = {}


def _sign_alt_variants():
    """{(znak, klasa broni): gest trybu alternatywnego} z make_signs.build_alt_gesture; None bez zrzutow W3."""
    import make_signs, make_pair
    if not _SIGN_ALT:
        grip = make_pair.grip_local(VFS, SK)
        for sign, clips in make_signs.ALT_SIGNS.items():
            for wc, clip in enumerate(clips):
                d = _sign_dump(clip)
                if d is None:
                    _SIGN_ALT.clear()
                    return None
                _SIGN_ALT[(sign, wc)] = make_signs.build_alt_gesture(d, SK, grip, wc > 0)
    return _SIGN_ALT


def test_sign_alt_gestures_reach_ground_in_place():
    # zgloszenie 23.10 "alternatywny Aard wystawia reke do przodu, a powinien w ziemie": gest trybu alternatywnego Aard /
    # Yrden = man_*_ground z W3 (bez broni, miecz rp, miecz lp) calym cialem w miejscu: korzen bez ruchu w poziomie, stopy
    # stoja, w klatce dotkniecia (ALT_BEFORE - chwila skutku w zDismember) przysiad i lewa dlon tuz nad ziemia przed
    # postacia, na koncu znow stoi (blokada ruchu konczy sie razem z gestem); wszystkie warianty tej samej dlugosci
    import make_signs, make_pair
    v = _sign_alt_variants()
    if v is None:
        print("  (pominiety - brak zrzutu W3)")
        return
    assert set(make_signs.ALT_SIGNS) == {"AARD", "YRDEN"}, sorted(make_signs.ALT_SIGNS)
    feet = (SK.idx("BIP01 L FOOT"), SK.idx("BIP01 R FOOT"))
    pel, hand = SK.idx("BIP01 PELVIS"), SK.idx("BIP01 L HAND")
    must = {"BIP01", "BIP01 PELVIS", "BIP01 L THIGH", "BIP01 R CALF", "BIP01 L FOOT", "BIP01 SPINE", "BIP01 L HAND", "ZS_RIGHTHAND"}
    for key, g in v.items():
        assert must <= {SK.names[i] for i in g["nodes"]}, key
        assert len(g["frames"]) == make_signs.ALT_BEFORE + make_signs.ALT_AFTER + 1, (key, len(g["frames"]))
        assert g["contact"] == make_signs.ALT_BEFORE and 2 <= g["release"] <= g["contact"], (key, g["release"])
        assert all(abs(r[0]) < 1e-6 and abs(r[2]) < 1e-6 for _, r in g["frames"]), key
        W0 = SK.compose(*g["frames"][0])
        slide = max(np.linalg.norm(SK.compose(*f)[i][:3, 3] - W0[i][:3, 3]) for f in g["frames"] for i in feet)
        assert slide < 0.5, (key, slide)
        Wc, We = SK.compose(*g["frames"][g["contact"]]), SK.compose(*g["frames"][-1])
        floor = min(W0[i][1, 3] for i in feet)
        assert Wc[hand][1, 3] - floor < 30.0, (key, Wc[hand][1, 3] - floor)                    # dlon przy ziemi
        assert Wc[hand][2, 3] - Wc[pel][2, 3] > 30.0, (key, Wc[hand][2, 3] - Wc[pel][2, 3])     # przed postacia
        assert W0[pel][1, 3] - Wc[pel][1, 3] > 15.0, (key, W0[pel][1, 3] - Wc[pel][1, 3])      # przysiad
        assert W0[pel][1, 3] - We[pel][1, 3] < 8.0, (key, W0[pel][1, 3] - We[pel][1, 3])       # wstal
    # dla porownania zwykly Aard (aard_front): dlon do przodu na wysokosci barku - stad zgloszenie
    old = make_signs.build_gesture(_sign_dump(make_signs.SIGNS["AARD"][0]), SK, make_pair.grip_local(VFS, SK), False, None, True)
    Wo = SK.compose(*old["frames"][old["release"] + 3])
    assert Wo[hand][1, 3] > 90.0, Wo[hand][1, 3]


def test_sign_alt_tables_match_zquicksign():
    # 6 nowych nazw z HUMANS (gesty strazy G1 - nikt ich nie gra albo tylko NPC), inne niz gesty trybu zwyklego; tablica
    # SIGN_ANIS_ALT [znak][klasa broni] i domyslne SignAltCastMs / SignAltGestureMs w zQuickSign.cpp jak w make_signs.py
    import re, make_signs, build_overlay
    defs = build_overlay.humans_ani_defs(VFS)
    alt = list(make_signs.SIGN_ANIS_ALT.values())
    used = list(make_signs.SIGN_ANIS.values()) + list(make_signs.SIGN_ANIS_STAND.values())
    assert len(alt) == 3 * len(make_signs.ALT_SIGNS) and len(set(alt)) == len(alt) and not set(alt) & set(used), alt
    assert all(n in defs and defs[n]["layer"] == 2 for n in alt), [n for n in alt if n not in defs]
    src = open(os.path.join(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"), "zQuickSign.cpp"), encoding="latin1").read()
    m = re.search(r"SIGN_ANIS_ALT\[SIGN_COUNT\]\[3\]\s*=\s*\{(.*?)\};", src, re.S)
    assert m, "brak SIGN_ANIS_ALT w zQuickSign.cpp"
    rows = [[None if x.strip() in ("0", "nullptr") else x.strip().strip('"') for x in r.split(",")]
            for r in re.findall(r"\{([^{}]*)\}", m.group(1))]
    want = [[make_signs.SIGN_ANIS_ALT.get((s, wc)) for wc in range(3)] for s in make_signs.QS_ORDER]
    assert rows == want, rows
    fps = retarget.FPS_OUT
    cast = ",".join(str(round(make_signs.ALT_BEFORE / fps * 1000)) if s in make_signs.ALT_SIGNS else "0" for s in make_signs.QS_ORDER)
    length = ",".join(str(round((make_signs.ALT_BEFORE + make_signs.ALT_AFTER + 1) / fps * 1000)) if s in make_signs.ALT_SIGNS
                      else "0" for s in make_signs.QS_ORDER)
    m1 = re.search(r'signAltCastMsText\[\d+\]\s*=\s*"([\d,]+)"', src)
    m2 = re.search(r'signAltGestureMsText\[\d+\]\s*=\s*"([\d,]+)"', src)
    assert m1 and m1.group(1) == cast and m2 and m2.group(1) == length, (m1 and m1.group(1), cast, m2 and m2.group(1), length)


def test_sign_alt_overlay_files_in_stand_overlay():
    # wygenerowane gesty alternatywne w nakladce postoju (zaklada ja zQuickSign na stale): warstwa LAYER, bez nastepnej,
    # wygaszenie BLEND_OUT_ALT, MAN calego ciala o dlugosci gestu
    import make_signs
    msb = os.path.join(make_signs.COMPILED, make_signs.OVERLAY_STAND + ".MSB")
    if not os.path.exists(msb):
        print("  (pominiety - brak nakladki, uruchom make_signs.py)")
        return
    anis = read_msb_anis(msb)
    for key, name in make_signs.SIGN_ANIS_ALT.items():
        assert name in anis, name
        a = anis[name]
        m = manlib.Man(open(os.path.join(make_signs.COMPILED, "%s-%s.MAN" % (make_signs.OVERLAY_STAND, name)), "rb").read())
        assert a.layer == make_signs.LAYER and m.layer == make_signs.LAYER and a.next == "", (name, a.layer, m.layer, a.next)
        assert m.frames == a.last_frame - a.first_frame + 1 == make_signs.ALT_BEFORE + make_signs.ALT_AFTER + 1, (name, m.frames)
        assert abs(a.blend_out - make_signs.BLEND_OUT_ALT) < 1e-5, (name, a.blend_out)
        assert {"BIP01", "BIP01 PELVIS", "BIP01 L THIGH", "BIP01 R FOOT"} <= {SK.names[i] for i in m.nodes}, name


def test_yrden_move_gesture_bends_toward_ground():
    # Yrden w trybie zwyklym w ruchu (tylko tulow i rece, nogi biegna): bez pochylenia dlon zostawala ok. pol metra nad
    # ziemia (wygladalo jak reka do przodu) - tulow pochylony z przysiadem W3 tak, ze najnizej dlon jest GROUND_REACH cm
    # nad stopa biegu; pierwsza klatka bez pochylenia (przejscie z biegu), kat w granicy; Znaki "do przodu" bez zmian
    import make_signs, make_pair
    v = _sign_variants()
    if v is None:
        print("  (pominiety - brak zrzutu W3)")
        return
    assert make_signs.GROUND_SIGNS == ("YRDEN",)
    for (sign, weapon), (st, mv) in v.items():
        if sign not in make_signs.GROUND_SIGNS:
            assert "bend" not in mv, sign
            continue
        assert 5.0 < mv["bend"] <= make_signs.GROUND_BEND_MAX, (sign, weapon, mv["bend"])
        assert mv["reach"] <= make_signs.GROUND_REACH + 1.0 and mv["reach0"] - mv["reach"] > 10.0, (mv["reach0"], mv["reach"])
    run = make_signs.mean_pose(VFS, SK)
    floor = make_signs.run_floor(VFS, SK)
    assert 5.0 < floor < 20.0, floor   # wezel stopy podparcia w biegu (srednia poza ma obie stopy w gorze)
    d = _sign_dump(make_signs.SIGNS["YRDEN"][0])
    flat = make_signs.build_gesture(d, SK, make_pair.grip_local(VFS, SK), False, run)
    mv = v[("YRDEN", False)][1]
    assert all(np.allclose(a, b) for a, b in zip(flat["frames"][0][0], mv["frames"][0][0]))


def _jump_attacks():
    """-> [(id, ustawienia, klatki z ruchem korzenia, klatka ciosu, szczyt wyskoku, garda)] albo None bez zrzutow W3."""
    import make_attacks, make_moves, make_pair
    grip = make_pair.grip_local(VFS, SK)
    out = []
    for mid, c in make_moves.JUMP_ATTACKS.items():
        d, g = _move_dump(c["clip"]), _guard(c["weapon"])   # garda stylu broni (2H: garda parady)
        if d is None or g is None:
            return None
        guard = g[0]
        m, hit, peak = make_moves.build_jump_variant(d, SK, grip, guard, c)
        out.append((mid, c, m, hit, peak, guard))
    return out


def test_jump_attack_leaps_and_chops_down():
    # atak z wyskoku (Rend W3, 1H z klipu _rp, 2H z _lp): wejscie z gardy W3, odbicie, wyskok (korzen w gore, obie stopy
    # nad ziemia), ladowanie tuz przed ciosem, ciecie z gory przed postacia, wypad do przodu, staly chwyt broni
    import make_moves, make_pair
    jumps = _jump_attacks()
    if jumps is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    gi = SK.idx("ZS_RIGHTHAND")
    grip = make_pair.grip_local(VFS, SK)
    for mid, c, m, hit, peak, guard in jumps:
        assert 6 < hit < len(m) - 5, (mid, hit, len(m))   # szybkie 1H od 0,3 s
        assert make_moves.JUMP_TAKEOFF < peak < hit, (mid, peak, hit)
        W = [SK.compose(l, r) for l, r in m]
        foot = lambda k: min(W[k][SK.idx(n)][:3, 3][1] for n in ("BIP01 L FOOT", "BIP01 R FOOT"))
        up = max(foot(k) for k in range(make_moves.JUMP_TAKEOFF, hit)) - foot(0)
        assert up > 0.6 * make_moves.JUMP_LIFT, (mid, "wyskok", up)
        assert abs(foot(hit) - foot(0)) < 8.0, (mid, "ladowanie przed ciosem", foot(hit), foot(0))
        tip = lambda k: W[k][gi][:3, 3] - W[k][gi][:3, 0] * make_pair.BLADE_CM[1]
        assert tip(hit)[1] < tip(hit - 2)[1] - 20.0, (mid, "ciecie z gory", tip(hit - 2)[1], tip(hit)[1])
        # wejscie z gardy bez przerzucania miecza nad glowa (sztych miedzy garda a klipem, nie lukiem w gore) - chyba ze
        # sam klip unosi w tej chwili miecz (fast_far_forward tnie od razu znad glowy): porownanie z klipem bez wejscia
        n = c["n"]
        raw, _, _ = make_moves.build_jump_variant(_move_dump(c["clip"]), SK, grip, None, c)
        raw_tip = lambda k: (lambda R: R[gi][:3, 3] - R[gi][:3, 0] * make_pair.BLADE_CM[1])(SK.compose(*raw[k]))
        tops = [max(tip(0)[1], tip(n)[1], raw_tip(k)[1]) + 30.0 for k in range(n + 1)]
        assert all(tip(k)[1] < tops[k] for k in range(n + 1)), (
            mid, "luk przy wejsciu", [round(tip(k)[1]) for k in range(n + 1)], [round(t) for t in tops])
        assert tip(hit)[2] - m[hit][1][2] > 40.0, (mid, "ostrze przed postacia", tip(hit)[2] - m[hit][1][2])
        assert m[hit][1][2] - m[0][1][2] > 100.0, (mid, "wypad", m[hit][1][2] - m[0][1][2])
        assert all(np.allclose(x, y, atol=1e-6) for x, y in zip(m[0][0], guard[0])), mid
        assert all(np.allclose(f[0][gi], grip, atol=1e-5) for f in m), mid
        # bez przeskokow w trakcie (kosc nie obraca sie o > 135 st. na klatke, korzen w pionie <= 20 cm / kl.)
        speed, _, _ = _motion_stats([m[0]] + m, make_moves.JOIN_BONES)
        assert max(speed) < 135.0, (mid, int(np.argmax(speed)), max(speed))
        ys = [r[1] for _, r in m]
        assert max(abs(ys[k + 1] - ys[k]) for k in range(len(ys) - 1)) < 20.0, mid


def test_jump_attack_reach_and_moves_ini():
    # zasieg ciecia z wyskoku (od voba do srodka klingi) jak ostrze kontry; w moves_w3.ini sekcje JUMPATK_F, _F2...
    # (1H) i JUMPATK2H_F, _F2... (2H) z hit=, reach=, overlayW3= (po make_moves.py)
    import make_moves
    jumps = _jump_attacks()
    if jumps is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    for mid, c, m, hit, peak, guard in jumps:
        reach = make_moves.counter_reach(SK, m, hit, "blade")
        assert 60.0 < reach < 200.0, (mid, reach)
    for base, w in (("JUMPATK_F", "1H"), ("JUMPATK2H_F", "2H")):
        ids = [mid for mid, c in make_moves.JUMP_ATTACKS.items() if c["weapon"] == w]
        assert ids == [base] + ["%s%d" % (base, k) for k in range(2, len(ids) + 1)], ids
    if not os.path.exists(make_moves.INI):
        print("  (bez sprawdzenia ini - brak moves_w3.ini)")
        return
    text = open(make_moves.INI, encoding="ascii").read()
    for mid in make_moves.JUMP_ATTACKS:
        assert "[%s]" % mid in text, mid
        sec = text.split("[%s]" % mid, 1)[1].split("\n[", 1)[0]
        for key in ("overlay=HUMANS_ZDF_MV_%s" % mid, "overlayW3=HUMANS_ZDF_MV_%s_W3" % mid, "hit=", "reach=", "traj="):
            assert key in sec, (mid, key)


def test_jump_attack_variants_by_weapon():
    # kilka wariantow ataku z wyskoku na bron (plugin losuje przy kazdym uzyciu): rozne klipy W3, 1H z postawy rp jak
    # styl 1H, 2H z lp jak styl 2H, bez wspolnych klipow; 1H lzejsze i szybsze - cios srednio wczesniej niz w 2H
    jumps = _jump_attacks()
    if jumps is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    by = {"1H": [], "2H": []}
    for mid, c, m, hit, peak, guard in jumps:
        by[c["weapon"]].append((c["clip"], hit))
    for w, side in (("1H", "_rp_"), ("2H", "_lp_")):
        clips = [cl for cl, _ in by[w]]
        assert len(clips) >= 3 and len(set(clips)) == len(clips), (w, clips)
        assert all(side in cl + "_" for cl in clips), (w, clips)
    assert not {cl for cl, _ in by["1H"]} & {cl for cl, _ in by["2H"]}
    mean = lambda w: sum(h for _, h in by[w]) / float(len(by[w]))
    assert mean("1H") < mean("2H"), by


def _assert_no_pose_pops(mid, m, limit):
    """Bez przeskokow pozy: kosc (JOIN_BONES, w ukladzie modelu) najwyzej limit st./kl. i bez pojedynczych szpilek
    predkosci (klatka ponad 70 st./kl. i 2,5x szybsza niz obie sasiednie tej samej kosci)."""
    import make_moves
    vel, _ = _world_speeds([m[0]] + m, make_moves.JOIN_BONES)
    sp = [[float(np.linalg.norm(v)) for v in vk] for vk in vel]
    assert max(max(s) for s in sp) < limit, (mid, max(max(s) for s in sp))
    for k in range(1, len(sp) - 1):
        for j, name in enumerate(make_moves.JOIN_BONES):
            assert sp[k][j] < 70.0 or sp[k][j] < 2.5 * max(sp[k - 1][j], sp[k + 1][j]), (
                mid, "przeskok", k, name, sp[k - 1][j], sp[k][j], sp[k + 1][j])


def test_jump_attack_variants_have_no_pose_pops():
    # warianty z klipow W3 bez przeskokow pozy (wejscie z gardy i sam klip - np. glowa w fast_7_rp skacze w 2. klatce)
    jumps = _jump_attacks()
    if jumps is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    for mid, c, m, hit, peak, guard in jumps:
        _assert_no_pose_pops(mid, m, 135.0)


def _run_attacks():
    """-> [(id, ustawienia, klatki z ruchem korzenia, klatka ciosu, poza truchtu W3)] albo None bez zrzutow W3."""
    import make_attacks, make_moves, make_pair
    run_dump = _loco_dump(make_attacks.RUN_POSE_CLIP)
    if run_dump is None:
        return None
    grip = make_pair.grip_local(VFS, SK)
    run = make_attacks.run_pose(run_dump, SK, grip)
    out = []
    for mid, c in make_moves.RUN_ATTACKS.items():
        d = _move_dump(c["clip"])
        if d is None:
            return None
        m, hit = make_moves.build_run_variant(d, SK, grip, run, c)
        out.append((mid, c, m, hit, run))
    return out


def test_run_attack_is_full_body_lunge_from_run_with_strike_ahead():
    # atak w biegu = ruch W3 calym cialem (plugin przejmuje EV_ATTACKRUN gry): start w pozie truchtu W3 i w ruchu (bez
    # zatrzymania biegu), nogi w animacji, wypad do przodu, cios ostrzem przed postacia najpozniej 0,25 s po ciosie
    # T_xHATTACKMOVE gry, bez przeskokow pozy, staly chwyt broni
    import make_attacks, make_moves, make_pair
    runs = _run_attacks()
    if runs is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    gi, th = SK.idx("ZS_RIGHTHAND"), SK.idx("BIP01 L THIGH")
    grip = make_pair.grip_local(VFS, SK)
    fps = retarget.FPS_OUT
    for mid, c, m, hit, run in runs:
        assert all(np.allclose(x, y, atol=1e-6) for x, y in zip(m[0][0], run[0])), mid
        gothic_hit, _ = make_attacks.humans_attack_timing(VFS, "T_%sATTACKMOVE" % c["weapon"])
        assert 0.3 < hit / fps <= gothic_hit / fps + 0.25, (mid, hit / fps, gothic_hit / fps)
        assert 6 < hit < len(m) - 5, (mid, hit, len(m))
        v0 = (m[4][1][2] - m[0][1][2]) / 4.0 * fps
        assert v0 > 150.0, (mid, "start w biegu", v0)
        assert m[hit][1][2] - m[0][1][2] > 100.0, (mid, "wypad", m[hit][1][2] - m[0][1][2])
        W = SK.compose(*m[hit])
        tip = W[gi][:3, 3] - W[gi][:3, 0] * make_pair.BLADE_CM[1]
        assert tip[2] - m[hit][1][2] > 50.0 and 30.0 < tip[1] < 230.0, (mid, "ostrze przed postacia", tip - m[hit][1])
        assert max(_ang(f[0][th], m[0][0][th]) for f in m) > 30.0, (mid, "nogi w animacji")
        assert all(np.allclose(f[0][gi], grip, atol=1e-5) for f in m), mid
        _assert_no_pose_pops(mid, m, 150.0)   # dlon w ciosie W3 do ok. 135 st./kl.
        ys = [r[1] for _, r in m]
        assert max(abs(ys[k + 1] - ys[k]) for k in range(len(ys) - 1)) < 20.0, mid


def test_run_attack_variants_near_and_far_by_weapon():
    # na bron dwa warianty ataku w biegu: z bliska i z daleka (plugin wybiera po odleglosci celu - wypad bez slizgu
    # nog); 1H szybkie (cios srednio wczesniej), 2H ciezkie; rozne klipy dla broni
    runs = _run_attacks()
    if runs is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    by = {"1H": [], "2H": []}
    for mid, c, m, hit, run in runs:
        by[c["weapon"]].append((mid, c["clip"], hit, m[hit][1][2] - m[0][1][2]))
    assert [x[0] for x in by["1H"]] == ["RUNATK_F", "RUNATK_F2"], by["1H"]
    assert [x[0] for x in by["2H"]] == ["RUNATK2H_F", "RUNATK2H_F2"], by["2H"]
    for w, v in by.items():
        lunges = sorted(x[3] for x in v)
        assert lunges[-1] > 1.6 * lunges[0], (w, lunges)
    assert not {x[1] for x in by["1H"]} & {x[1] for x in by["2H"]}
    mean = lambda w: sum(x[2] for x in by[w]) / float(len(by[w]))
    assert mean("1H") < mean("2H"), by


def test_run_attack_moves_ini_and_full_body_overlay():
    # po make_moves.py: sekcje RUNATK_* w moves_w3.ini z overlay=, overlayW3= (koniec w gardzie W3 - atak w biegu
    # tylko w stylu W3), hit=, reach= (zasieg ostrza), traj=, bez nietykalnosci; nakladka calym cialem (nogi tez)
    import make_moves
    if not os.path.exists(make_moves.INI):
        print("  (pominiety - brak moves_w3.ini)")
        return
    text = open(make_moves.INI, encoding="ascii").read()
    for mid in make_moves.RUN_ATTACKS:
        assert "[%s]" % mid in text, mid
        sec = text.split("[%s]" % mid, 1)[1].split("\n[", 1)[0]
        for key in ("overlay=HUMANS_ZDF_MV_%s\n" % mid, "overlayW3=HUMANS_ZDF_MV_%s_W3\n" % mid, "hit=", "reach=", "traj=",
                    "iframes=0,0\n"):
            assert key in sec, (mid, key)
        reach = float(sec.split("reach=", 1)[1].split("\n", 1)[0])
        assert 50.0 < reach < 220.0, (mid, reach)   # ciezkie ciecia W3 koncza sie blisko (strong_4_lp ok. 59 cm)
        man = os.path.join(make_moves.COMPILED, "HUMANS_ZDF_MV_%s_W3-T_1HPARADEJUMPB.MAN" % mid)
        names = {SK.names[i] for i in manlib.Man(open(man, "rb").read()).nodes}
        assert {"BIP01", "BIP01 L THIGH", "BIP01 R FOOT", "BIP01 R HAND"} <= names, (mid, sorted(names))


def _turn_deg(a, b):
    """Najmniejszy kat (st.) miedzy kierunkami a i b."""
    return abs((a - b + 180.0) % 360.0 - 180.0)


def test_moves_roll_back_ends_facing_enemy_without_spin():
    # przewrot w tyl (ROLL_B) konczy sie przodem do przeciwnika jak na starcie - obrot jest w animacji W3 (przy
    # wstawaniu), a nie w doklejonym przejsciu do postawy (stary ROLL_B = przewrot w przod obrocony o 180 st.: koniec
    # tylem do wroga i ok. 160 st. obrotu w miejscu w 0,24 s)
    import make_moves, make_pair, make_attacks, recovery
    d, g = _move_dump(make_moves.ROLL_B_CLIP), _move_dump(make_attacks.STYLES["1H"]["L"])
    if d is None or g is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    grip = make_pair.grip_local(VFS, SK)
    frames, motion, traj, iframes, cancel = make_moves.build_roll_back(d, SK, grip)
    x, z = traj[-1]
    assert z < -350.0 and abs(x) < 0.25 * abs(z), ("przewrot w tyl nie idzie w tyl", x, z)
    yaw = lambda R: float(np.degrees(make_pair.body_yaw(R, SK)))
    start, end = yaw(frames[0][0][0]), yaw(frames[-1][0][0])
    assert _turn_deg(start, end) < 30.0, ("koniec obrocony wzgledem startu", start, end)
    guard = make_attacks.guard_loop(g, SK, grip)[0]
    for name, target in (("garda W3", guard), ("S_1HRUN", recovery.stance_pose(VFS, SK, "S_1HRUN"))):
        assert _turn_deg(end, yaw(target[0][0])) < 75.0, ("przejscie do postawy obraca w miejscu", name, end)
    # bohater juz na nogach (ostatnie klatki): bez szybkiego obrotu
    tail = [yaw(l[0]) for l, _ in frames[-8:]]
    assert max(_turn_deg(a, b) for a, b in zip(tail, tail[1:])) < 20.0, tail
    assert 0.0 < iframes[0] < iframes[1] <= cancel, (iframes, cancel)


def test_moves_roll_back_shortens_only_feet_up_phase():
    # klip W3 ma 7 m i szczyt 14 m/s (kamera Gothica nie nadaza) - skrocona tylko faza ze stopami w gorze (skok
    # i przewrot); kroki na ziemi (start, obrot przy wstawaniu) bez zmian = stopy sie nie slizgaja; droga jak przewrot
    # w przod (~5 m)
    import make_moves, make_pair
    d = _move_dump(make_moves.ROLL_B_CLIP)
    if d is None:
        print("  (pominiety - brak zrzutu W3)")
        return
    grip = make_pair.grip_local(VFS, SK)
    _, full, traj_full, _, _ = make_moves.build_move_full(d, SK, grip, 1.0, 0.7, 0.0, 0.95)
    _, motion, traj, _, _ = make_moves.build_roll_back(d, SK, grip)
    up = make_moves.feet_up(full, SK)
    assert any(up) and not up[0] and not up[-1], up
    step = lambda t, k: float(np.hypot(t[k][0] - t[k - 1][0], t[k][1] - t[k - 1][1]))
    for k in range(1, min(len(traj), len(traj_full))):
        want = step(traj_full, k) * (make_moves.ROLL_B_AIR if up[k] and up[k - 1] else 1.0)   # odbicie / ladowanie pelne
        assert abs(step(traj, k) - want) < 0.05, (k, up[k], step(traj, k), want)
    dist = float(np.hypot(*traj[-1]))
    assert 420.0 < dist < 600.0, dist
    assert max(step(traj, k) for k in range(1, len(traj))) < 40.0, "przewrot w tyl za szybki dla kamery"
    # ruch z animacji (motion) ma ten sam tor co ini
    base = motion[0][1]
    for (tx, tz), (_, r) in zip(traj, motion):
        assert abs(tx - (r[0] - base[0])) < 1e-3 and abs(tz - (r[2] - base[2])) < 1e-3


def test_air_compress_keeps_heights_and_first_frame():
    # skracanie toru nie rusza wysokosci korzenia ani obrotow kosci; scale 1 = bez zmian
    import make_moves, make_pair
    d = _move_dump(make_moves.ROLL_B_CLIP)
    if d is None:
        print("  (pominiety - brak zrzutu W3)")
        return
    grip = make_pair.grip_local(VFS, SK)
    _, full, _, _, _ = make_moves.build_move_full(d, SK, grip, 1.0, 0.7, 0.0, 0.95)
    same = make_moves.air_compress(full, SK, 1.0)
    half = make_moves.air_compress(full, SK, 0.5)
    for (l0, r0), (l1, r1), (l2, r2) in zip(full, same, half):
        assert np.allclose(r0, r1) and abs(r0[1] - r2[1]) < 1e-6
        assert all(np.allclose(a, b) for a, b in zip(l0, l2))
    assert np.allclose(full[0][1], half[0][1])
    assert np.hypot(half[-1][1][0] - half[0][1][0], half[-1][1][2] - half[0][1][2]) < \
        np.hypot(full[-1][1][0] - full[0][1][0], full[-1][1][2] - full[0][1][2])


def test_moves_ini_roll_back_from_turning_clip():
    # po make_moves.py: ROLL_B z klipu W3 z obrotem przodem (nie obrocony przewrot w przod), tor w tyl, bez skokow
    import make_moves
    if not os.path.exists(make_moves.INI):
        print("  (pominiety - brak moves_w3.ini)")
        return
    text = open(make_moves.INI, encoding="ascii").read()
    sec = text.split("[ROLL_B]", 1)[1].split("\n[", 1)[0]
    assert make_moves.ROLL_B_CLIP in sec, sec[-200:]
    pts = [tuple(float(v) for v in p.split(",")) for p in sec.split("traj=", 1)[1].split("\n", 1)[0].split()]
    x, z = pts[-1]
    assert z < -350.0 and abs(x) < 0.25 * abs(z), (x, z)
    assert max(float(np.hypot(b[0] - a[0], b[1] - a[1])) for a, b in zip(pts, pts[1:])) < 40.0
    for mid in ("ROLL_F", "ROLL_L", "ROLL_R"):
        assert "[%s]" % mid in text and " + ".join(make_moves.ROLL) in text.split("[%s]" % mid, 1)[1].split("\n[", 1)[0], mid


def _guard(w, n=None):
    """Petla gardy stylu W3 (make_attacks.style_guard) albo None bez zrzutu W3."""
    import make_attacks, make_pair
    d = _move_dump(make_attacks.guard_clip(w))
    if d is None:
        return None
    grip = make_pair.grip_local(VFS, SK)
    return make_attacks.style_guard(d, SK, grip, w) if n is None else make_attacks.style_guard(d, SK, grip, w, n)


def test_w3_guard_2h_is_parry_idle_loop():
    # garda 2H stylu W3 = garda parady Geralta: prawdziwa petla man_geralt_sword_parry_idle_lp (1,0 s = 25 kl. @25), miecz
    # uniesiony przed soba; wariant lp jak ciecia 2H (strong_*_lp), parady 2H i ataki z wypadem 2H. Petla domknieta bez
    # szwu (ostatnia -> pierwsza jak sasiednie klatki, kosci i korzen), w miejscu, staly chwyt; 1. klatka = klip W3
    import make_attacks, make_moves, make_pair
    assert make_attacks.STYLES["2H"]["guard"] == "man_geralt_sword_parry_idle_lp"
    assert make_attacks.guard_clip("2H") == "man_geralt_sword_parry_idle_lp"
    frames = _guard("2H")
    if frames is None:
        print("  (pominiety - brak zrzutu W3)")
        return
    n = len(frames)
    assert n == 25, n
    steps = [max(_ang(a, b) for a, b in zip(frames[k][0], frames[(k + 1) % n][0])) for k in range(n)]
    assert steps[-1] <= 1.5 * max(steps[:-1]) + 0.05, (steps[-1], max(steps[:-1]))
    rsteps = [float(np.linalg.norm(frames[(k + 1) % n][1] - frames[k][1])) for k in range(n)]
    assert rsteps[-1] <= 1.5 * max(rsteps[:-1]) + 0.05, (rsteps[-1], max(rsteps[:-1]))
    assert abs(frames[0][1][0]) < 1e-6 and abs(frames[0][1][2]) < 1e-6, frames[0][1]
    # kolysanie miednicy W3 (przenoszenie ciezaru, ok. 5 cm do przodu) zostaje - inaczej stopy slizgalyby sie
    assert all(abs(r[0]) < 6.0 and abs(r[2]) < 6.0 for _, r in frames), "garda nie stoi w miejscu"
    grip = make_pair.grip_local(VFS, SK)
    gi = SK.idx("ZS_RIGHTHAND")
    assert all(np.allclose(f[0][gi], grip, atol=1e-5) for f in frames)
    raw = retarget.retarget(make_moves.to_entity_start(_move_dump(make_attacks.guard_clip("2H"))), SK, grip, False, "w3",
                            weapon="r_weapon")[0]
    assert all(np.allclose(a, b, atol=1e-5) for a, b in zip(frames[0][0], raw[0]))
    # miecz uniesiony przed soba: dlonie na wysokosci piersi przed postacia, ostrze wprost do przodu (ok. 1,5 m przed)
    W = SK.compose(*frames[0])
    hand = W[gi][:3, 3]
    tip = hand - W[gi][:3, 0] * make_pair.BLADE_CM[1]
    assert hand[1] > 120.0 and hand[2] > 40.0 and tip[2] > hand[2] + 60.0 and tip[1] > 90.0, (hand, tip)


def test_w3_guard_1h_unchanged():
    # garda 1H bez zmian: 1. klatka ciecia L z oddechem (guard_loop), 60 klatek
    import make_attacks, make_pair
    assert "guard" not in make_attacks.STYLES["1H"] and make_attacks.guard_clip("1H") == make_attacks.STYLES["1H"]["L"]
    d = _move_dump(make_attacks.STYLES["1H"]["L"])
    if d is None:
        print("  (pominiety - brak zrzutu W3)")
        return
    grip = make_pair.grip_local(VFS, SK)
    a, b = make_attacks.style_guard(d, SK, grip, "1H"), make_attacks.guard_loop(d, SK, grip)
    assert len(a) == len(b) == make_attacks.GUARD_FRAMES
    assert all(np.allclose(x[1], y[1]) and all(np.allclose(p, q) for p, q in zip(x[0], y[0])) for x, y in zip(a, b))


def _smooth_joins(tag, frames, joins, srcs, n):
    """Sklejenia bez przeskokow: w oknie kazdego sklejenia (joins = indeksy pierwszych klatek nowej czesci, od klatki
    przed do n + 1 po) kosc JOIN_BONES najwyzej 135 st./kl., przyspieszenie kosci i korzenia nie wieksze niz w samych
    klipach W3 srcs (x1,1). Przed animacja i po niej postac stoi (klatki skrajne powtorzone) - ruszenie z gardy i
    zatrzymanie w gardzie tez sa sklejeniami. -> pomiary [(sklejenie, max predkosc, max przysp., max przysp. korzenia)]."""
    import make_moves
    a, r = [], []
    for s in srcs:
        _, acc, racc = _motion_stats(s, make_moves.JOIN_BONES)
        a += acc
        r += racc
    src_acc, src_racc = max(a), max(r)
    speed, acc, racc = _motion_stats([frames[0]] + list(frames) + [frames[-1]], make_moves.JOIN_BONES)
    out = []
    for j in joins:
        win = range(max(0, j - 1), min(len(acc), j + n + 2))
        sp, ac, rc = max(speed[k] for k in win), max(acc[k] for k in win), max(racc[k] for k in win)
        assert sp < 135.0, (tag, j, "predkosc", sp)
        assert ac <= 1.1 * src_acc, (tag, j, "przyspieszenie", ac, src_acc)
        assert rc <= 1.1 * src_racc + 0.5, (tag, j, "korzen", rc, src_racc)
        out.append((j, round(sp, 1), round(ac, 1), round(rc, 2)))
    return out


def _same_pose(a, b, atol=1e-5):
    return all(np.allclose(x, y, atol=atol) for x, y in zip(a[0], b[0]))


def _exit_smooth(tag, frames, n_motion, srcs, max_turn=30.0):
    """Koniec w gardzie parady 2H (make_moves.guard_recovery): okno od najwczesniejszego startu przejscia (n_motion - 1 -
    GUARD_OVERLAP) do konca jak _smooth_joins; obrot calej postaci (BIP01) najwyzej max_turn st./kl. -> pomiary."""
    import make_moves, make_pair
    j = max(0, n_motion - 1 - make_moves.GUARD_OVERLAP)
    out = _smooth_joins(tag, frames, [j], srcs, make_moves.REC_FRAMES + make_moves.GUARD_OVERLAP)
    yaw = [float(np.degrees(make_pair.body_yaw(l[0], SK))) for l, _ in frames[j:]]
    turn = max(_turn_deg(a, b) for a, b in zip(yaw, yaw[1:]))
    assert turn < max_turn, (tag, "obrot w miejscu", turn)
    return out + [("obrot", round(turn, 1))]


def test_w3_2h_cuts_enter_from_parry_guard_and_return():
    # ciecia stylu 2H (T_2HATTACKL/R): klatka 0 = garda parady, wejscie w zamach bezwladnosciowo (ENTRY_FRAMES, bez
    # dodatkowych klatek - dlugosc i trafienie jak bez wejscia), koniec bezwladnosciowo w gardzie; bez przeskokow na
    # sklejeniach (stare wejscie z 1. klatki ciecia: 141 st./kl.^2 i 16 cm skoku korzenia), staly chwyt
    import make_attacks, make_pair
    guard = _guard("2H")
    if guard is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    guard = guard[0]
    grip = make_pair.grip_local(VFS, SK)
    gi = SK.idx("ZS_RIGHTHAND")
    st = make_attacks.STYLES["2H"]
    for side in ("L", "R"):
        d = _move_dump(st[side])
        frames, hit, _ = make_attacks.style_attack(d, SK, grip, st, side, guard)
        plain, plain_hit = make_attacks.build_attack(d, SK, grip, st["step"], windup=st.get("windup"))
        assert hit == plain_hit and len(frames) == len(plain) + make_attacks.REC_FRAMES, (side, hit, plain_hit)
        assert _same_pose(frames[0], guard) and abs(frames[0][1][1] - guard[1][1]) < 1e-6, side
        assert _same_pose(frames[-1], guard, 1e-6) and abs(frames[-1][1][1] - guard[1][1]) < 1e-6, side
        assert all(np.allclose(f[0][gi], grip, atol=1e-5) for f in frames), side
        src, _ = make_attacks.prep_attack(d, SK, grip, st.get("windup"))
        print("   2H %s wejscie:" % side, _smooth_joins(side, frames, [0], [src], make_attacks.ENTRY_FRAMES))
        print("   2H %s koniec:" % side, _exit_smooth(side, frames, len(plain), [src]))
        # ciecie bez zmian po wejsciu (od klatki ENTRY_FRAMES) - ta sama poza co bez gardy
        k = make_attacks.ENTRY_FRAMES
        assert _same_pose(frames[k], plain[k]) and np.allclose(frames[k][1], plain[k][1]), side


def test_w3_2h_combo_enters_from_parry_guard():
    # kombinacja S_2HATTACK: wejscie z gardy parady jak ciecia (zdarzenia i dlugosc bez zmian), powrot do gardy
    # bezwladnosciowo, bez przeskokow na wejsciu i na koncu
    import make_attacks, make_pair
    guard = _guard("2H")
    st = make_attacks.STYLES["2H"]
    dumps = {c: _move_dump(c) for c in st["combo"]}
    if guard is None or any(d is None for d in dumps.values()):
        print("  (pominiety - brak zrzutow W3)")
        return
    guard = guard[0]
    grip = make_pair.grip_local(VFS, SK)
    first = make_attacks.combo_first(st)
    frames, ev, _ = make_attacks.style_combo(dumps, SK, grip, st, guard)
    plain, pev = make_attacks.build_combo(dumps, SK, grip, st["combo_step"], st["hits"], st["start"],
                                          windup=st.get("windup"), first=first)
    assert ev["opt"] == pev["opt"] and ev["clips"] == pev["clips"] and ev["end"][:-1] == pev["end"][:-1], (ev, pev)
    assert ev["end"][-1] == ev["win"][-1] == len(frames) == len(plain) + make_attacks.REC_FRAMES, ev
    assert _same_pose(frames[0], guard) and _same_pose(frames[-1], guard, 1e-6)
    # otwarcie kombo (combo_open): pierwsze ciecie spoza zestawu Geralta, z wlasnym zamachem - cios w 0,28 s
    if first:
        assert ev["clips"][0] == first[0] and ev["opt"][0] <= 8, ev
    src_dump = lambda c: (first[1], first[2]) if first and c == first[0] else (dumps[c], st.get("windup"))
    srcs = [make_attacks.prep_attack(src_dump(c)[0], SK, grip, src_dump(c)[1])[0] for c in ev["clips"]]
    print("   kombinacja 2H wejscie:", _smooth_joins("combo", frames, [0], srcs, make_attacks.ENTRY_FRAMES))
    print("   kombinacja 2H koniec:", _exit_smooth("combo", frames, len(plain), srcs))
    gi = SK.idx("ZS_RIGHTHAND")
    assert all(np.allclose(f[0][gi], grip, atol=1e-5) for f in frames)


def test_w3_2h_parades_start_and_end_in_parry_guard():
    # blok 2H (T_2HPARADE_0/_A2/_A3, odbicia parry_f_*_lp - zaczynaja sie prawie w parry_idle_lp): z gardy parady, z
    # powrotem do niej; korzen bez skoku predkosci na wejsciu (stary chain: wysokosc klipu od razu, 7,5 cm/kl.^2)
    import make_attacks, make_pair
    guard = _guard("2H")
    if guard is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    guard = guard[0]
    grip = make_pair.grip_local(VFS, SK)
    st = make_attacks.STYLES["2H"]
    for clip in make_attacks.PARADES["2H"]:
        d = _move_dump(clip)
        frames = make_attacks.build_parade(d, SK, grip, guard, st["step"], smooth=True)
        assert 12 <= len(frames) <= 30, len(frames)
        assert _same_pose(frames[0], guard) and abs(frames[0][1][1] - guard[1][1]) < 1e-6, clip
        assert _same_pose(frames[-1], guard, 1e-6) and abs(frames[-1][1][1] - guard[1][1]) < 1e-6, clip
        src, _ = make_attacks.prep_attack(d, SK, grip)
        rec = len(frames) - 1 - make_attacks.REC_FRAMES
        print("   %s:" % clip[-14:], _smooth_joins(clip, frames, [1, rec], [src], 4))


def test_w3_2h_counters_enter_from_parry_guard_and_end_in_it():
    # kontry w stylu 2H (T_2HPARADEJUMPB, wariant _W3): wejscie z gardy parady w odbicie, odbicie -> riposta / repel,
    # koniec w gardzie parady; bez przeskokow, staly chwyt; chwila ciosu jak w 1H (jedno hit= w ini dla obu broni)
    import make_attacks, make_moves, make_pair
    guard, g1 = _guard("2H"), _guard("1H")
    if guard is None or g1 is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    guard, g1 = guard[0], g1[0]
    grip = make_pair.grip_local(VFS, SK)
    gi = SK.idx("ZS_RIGHTHAND")
    for mid, c in make_moves.COUNTERS.items():
        p, a = _move_dump(c["parry"]), _move_dump(c["attack"])
        args = (c.get("hit"), c.get("start"), c.get("step"))
        m, hit, joins = make_moves.build_counter(p, a, SK, grip, *args, entry=guard)
        _, hit1, _ = make_moves.build_counter(p, a, SK, grip, *args, entry=g1)
        assert hit == hit1, (mid, hit, hit1)
        full = make_moves.guard_recovery(SK, m, guard, make_moves.REC_FRAMES)   # jak wariant _W3 w write_move
        assert _same_pose(full[0], guard, 1e-6) and _same_pose(full[-1], guard, 1e-6), mid
        assert all(np.allclose(f[0][gi], grip, atol=1e-5) for f in full), mid
        pf, _ = make_attacks.prep_attack(p, SK, grip)
        af, _ = make_attacks.prep_attack(a, SK, grip)
        print("   %s 2H:" % mid, _smooth_joins(mid, full, [0] + joins, [pf, af],
                                               max(make_moves.COUNTER_ENTRY, make_moves.COUNTER_BLEND)),
              _exit_smooth(mid, full, len(m), [pf, af]))


def test_w3_2h_lunges_end_in_parry_guard():
    # ataki z wyskoku 2H (wejscie z gardy parady) i w biegu 2H (wejscie z truchtu W3): koniec wariantu _W3 w gardzie
    # parady bez przeskoku (inertial_recovery jak w write_move)
    import make_attacks, make_moves, make_pair
    guard = _guard("2H")
    jumps, runs = _jump_attacks(), _run_attacks()
    if guard is None or jumps is None or runs is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    guard = guard[0]
    grip = make_pair.grip_local(VFS, SK)
    items = [(mid, c, m) for mid, c, m, hit, peak, g in jumps] + [(mid, c, m) for mid, c, m, hit, run in runs]
    for mid, c, m in items:
        if c["weapon"] != "2H":
            continue
        full = make_moves.guard_recovery(SK, m, guard, make_moves.REC_FRAMES)   # jak wariant _W3 w write_move
        assert _same_pose(full[-1], guard, 1e-6), mid
        print("   %s koniec:" % mid, _exit_smooth(mid, full, len(m), [m]))
        if mid.startswith("JUMPATK"):   # wejscie z gardy parady: przyspieszenie jak w samym klipie W3
            raw, _, _ = make_moves.build_jump_variant(_move_dump(c["clip"]), SK, grip, None, c)
            assert _same_pose(full[0], guard, 1e-6), mid
            print("   %s wejscie:" % mid, _smooth_joins(mid, full, [0], [raw], c["n"]))


def test_w3_2h_dodges_and_rolls_end_in_parry_guard():
    # uniki i przewroty w stylu 2H (wariant _W3, T_2HPARADEJUMPB): koniec w gardzie parady bez przeskoku i bez szybkiego
    # obrotu w miejscu (ruchy W3 koncza w postawie rp, garda lp - obrot rozlozony przez guard_recovery)
    import make_moves, make_pair
    guard = _guard("2H")
    if guard is None or any(_move_dump(c) is None for c in list(make_moves.DODGES.values()) + list(make_moves.ROLL)):
        print("  (pominiety - brak zrzutow W3)")
        return
    guard = guard[0]
    grip = make_pair.grip_local(VFS, SK)
    moves = [("DODGE_" + k, make_moves.build_move_full(_move_dump(c), SK, grip, 1.0, 0.5)) for k, c in make_moves.DODGES.items()]
    roll = make_moves.concat([_move_dump(c) for c in make_moves.ROLL])
    moves += [("ROLL_" + k, make_moves.build_move_full(roll, SK, grip, 1.0, make_moves.ROLL_IFRAMES, y, make_moves.ROLL_CANCEL))
              for k, y in make_moves.ROLL_YAW.items()]
    if _move_dump(make_moves.ROLL_B_CLIP) is not None:
        moves.append(("ROLL_B", make_moves.build_roll_back(_move_dump(make_moves.ROLL_B_CLIP), SK, grip)))
    for mid, (frames, motion, traj, iframes, cancel) in moves:
        full = make_moves.guard_recovery(SK, frames, guard, make_moves.REC_FRAMES)
        assert len(full) == len(frames) + make_moves.REC_FRAMES and _same_pose(full[-1], guard, 1e-6), mid
        print("   %s:" % mid, _exit_smooth(mid, full, len(frames), [frames]))


def test_guard_recovery_spreads_turn_into_guard_without_changing_length():
    # koniec w gardzie parady 2H (lp) po ruchu konczacym w postawie rp: przejscie moze zaczac sie do GUARD_OVERLAP klatek
    # przed koncem (ta sama dlugosc animacji - cancel i tor w ini bez zmian), wybor po najmniejszym szarpnieciu - nigdy
    # gorzej niz zwykle inertial_recovery; ostatnia klatka = garda, poczatek ruchu bez zmian
    import make_moves, make_pair
    guard = _guard("2H")
    c = make_moves.COUNTERS["COUNTER_R"]
    p, a = _move_dump(c["parry"]), _move_dump(c["attack"])
    if guard is None or p is None or a is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    guard = guard[0]
    grip = make_pair.grip_local(VFS, SK)
    m, _, _ = make_moves.build_counter(p, a, SK, grip, entry=guard)
    m = make_pair.inplace(m)
    n, ov = make_moves.REC_FRAMES, make_moves.GUARD_OVERLAP
    plain = make_moves.inertial_recovery(m, guard, n)
    spread = make_moves.guard_recovery(SK, m, guard, n)
    assert len(spread) == len(plain) == len(m) + n
    assert _same_pose(spread[-1], guard, 1e-6) and abs(spread[-1][1][1] - guard[1][1]) < 1e-6
    k0 = len(m) - ov
    assert all(_same_pose(x, y, 1e-9) for x, y in zip(spread[:k0], m[:k0]))
    lo = len(m) - ov - 2
    cost = lambda fr: make_moves.guard_blend_cost(SK, fr, lo)
    assert cost(spread) <= cost(plain) + 1e-6, (cost(spread), cost(plain))
    # COUNTER_R konczy obrocony o ok. 160 st. wzgledem gardy - obrot ciala na klatke wyraznie mniejszy
    yaw = lambda fr: [float(np.degrees(make_pair.body_yaw(l[0], SK))) for l, _ in fr[lo:]]
    turn = lambda fr: max(_turn_deg(x, y) for x, y in zip(yaw(fr), yaw(fr)[1:]))
    assert turn(spread) < 0.8 * turn(plain), (turn(spread), turn(plain))


def _man_frames(path):
    m = manlib.Man(open(path, "rb").read())
    return [_sign_pose(m, k) for k in range(m.frames)]


def test_w3_generated_files_use_parry_guard_2h():
    # po make_attacks.py / make_moves.py / make_loco.py: postawa S_2HRUN stylu = petla parady (25 kl.), ciecia, kombinacja i
    # parady 2H zaczynaja sie i koncza w gardzie parady; warianty _W3 ruchow koncza T_2HPARADEJUMPB w gardzie 2H, a
    # T_1HPARADEJUMPB w gardzie 1H; przejscia biegu 2H stylu W3 z gardy / do gardy parady
    import make_attacks, make_moves, make_pair
    guard2, guard1 = _guard("2H"), _guard("1H")
    ovl = os.path.join(make_attacks.COMPILED, "HUMANS_ZDF_W3_2H-S_2HRUN.MAN")
    if guard2 is None or guard1 is None or not os.path.exists(ovl):
        print("  (pominiety - brak nakladek, uruchom make_attacks.py / make_moves.py / make_loco.py)")
        return
    names = ("BIP01", "BIP01 PELVIS", "BIP01 SPINE2", "BIP01 HEAD", "BIP01 R HAND", "BIP01 L HAND", "BIP01 L FOOT",
             "BIP01 R FOOT", "ZS_RIGHTHAND")

    def pts(pose):
        W = SK.compose(pose[0], np.array([0.0, pose[1][1], 0.0]))
        return np.array([W[SK.idx(n)][:3, 3] for n in names])

    def near(pose, want, tag, tol=1.5):
        d = float(np.abs(pts(pose) - pts(want)).max())
        assert d < tol, (tag, d)

    loop = _man_frames(ovl)
    assert len(loop) == len(guard2), len(loop)
    for k in (0, 7, 18):
        near(loop[k], guard2[k], ("S_2HRUN", k))
    g2, g1 = guard2[0], guard1[0]
    for ani in ("T_2HATTACKL", "T_2HATTACKR", "S_2HATTACK", "T_2HPARADE_0", "T_2HPARADE_0_A2", "T_2HPARADE_0_A3"):
        fr = _man_frames(os.path.join(make_attacks.COMPILED, "HUMANS_ZDF_W3_2H-%s.MAN" % ani))
        near(fr[0], g2, (ani, "start"))
        near(fr[-1], g2, (ani, "koniec"))
    text = open(make_moves.INI, encoding="ascii").read()
    w3 = [l.split("=", 1)[1] for l in text.splitlines() if l.startswith("overlayW3=")]
    assert len(w3) >= 20, len(w3)
    tail = make_moves.REC_FRAMES + make_moves.GUARD_OVERLAP + 2
    for o in w3:
        fr2 = _man_frames(os.path.join(make_moves.COMPILED, "%s-T_2HPARADEJUMPB.MAN" % o))
        near(fr2[-1], g2, (o, "2H"))
        near(_man_frames(os.path.join(make_moves.COMPILED, "%s-T_1HPARADEJUMPB.MAN" % o))[-1], g1, (o, "1H"))
        # koniec 2H: bez przeskoku i bez szybkiego obrotu w miejscu do gardy parady
        speed, _, _ = _motion_stats(fr2[-tail:] + [fr2[-1]], make_moves.JOIN_BONES)
        yaw = [float(np.degrees(make_pair.body_yaw(l[0], SK))) for l, _ in fr2[-tail:]]
        assert max(speed) < 135.0 and max(_turn_deg(a, b) for a, b in zip(yaw, yaw[1:])) < 30.0, (o, max(speed))
    for ovl, ani in (("HUMANS_ZDF_W3_LOCO_2H", "T_2HRUN_2_2HRUNL"), ("HUMANS_ZDF_W3_LOCO_2H", "T_2HWALK_2_2HWALKL"),
                     ("HUMANS_ZDF_W3_SPRINT_2H", "T_2HRUN_2_2HRUNL")):
        p = os.path.join(make_attacks.COMPILED, "%s-%s.MAN" % (ovl, ani))
        if os.path.exists(p):
            near(_man_frames(p)[0], g2, (ovl, ani))
    for ovl, ani in (("HUMANS_ZDF_W3_LOCO_2H", "T_2HRUNL_2_2HRUN"), ("HUMANS_ZDF_W3_LOCO_2H", "T_2HWALKL_2_2HWALK"),
                     ("HUMANS_ZDF_W3_SPRINT_2H", "T_2HRUNL_2_2HRUN")):
        p = os.path.join(make_attacks.COMPILED, "%s-%s.MAN" % (ovl, ani))
        if os.path.exists(p):
            near(_man_frames(p)[-1], g2, (ovl, ani))



def test_loco_magic_mode_uses_w3_unarmed_frames():
    # tryb magii (czar w reku, zgloszenie 21.10): HUMANS_ZDF_W3_LOCO / _SPRINT maja S_MAGRUNL / S_MAGWALKL z tymi samymi
    # klatkami co S_RUNL / S_WALKL, przejscia T_MAG*_2_MAG*L z postawy S_MAGRUN i T_MAG*L_2_MAG* do niej (ostatnia
    # klatka = postawa); definicje (warstwa 1, nastepna) jak w HUMANS, a rzucanie (T_MAGRUN_2_*CAST / *SHOOT) zostaje z gry
    import make_loco, build_overlay, recovery
    defs = build_overlay.humans_ani_defs(VFS)
    pairs = (("HUMANS_ZDF_W3_LOCO", "S_MAGRUNL", "S_RUNL"), ("HUMANS_ZDF_W3_LOCO", "S_MAGWALKL", "S_WALKL"),
             ("HUMANS_ZDF_W3_SPRINT", "S_MAGRUNL", "S_RUNL"))
    if not all(os.path.exists(os.path.join(make_loco.COMPILED, "%s-%s.MAN" % (o, a))) for o, a, _ in pairs):
        print("  (pominiety - brak nakladek, uruchom make_loco.py)")
        return
    for o, mag, base in pairs:
        a = _man_frames(os.path.join(make_loco.COMPILED, "%s-%s.MAN" % (o, mag)))
        b = _man_frames(os.path.join(make_loco.COMPILED, "%s-%s.MAN" % (o, base)))
        assert len(a) == len(b), (o, mag, len(a), len(b))
        for x, y in zip(a, b):
            assert all(np.allclose(p, q, atol=1e-6) for p, q in zip(x[0], y[0])) and np.allclose(x[1], y[1], atol=1e-4), (o, mag)
    idle = recovery.stance_pose(VFS, SK, "S_MAGRUN")
    body = [SK.idx(n) for n in ("BIP01 SPINE1", "BIP01 L UPPERARM", "BIP01 R UPPERARM", "BIP01 L FOREARM", "BIP01 R FOREARM")]
    for o, start, stop in (("HUMANS_ZDF_W3_LOCO", "T_MAGRUN_2_MAGRUNL", "T_MAGRUNL_2_MAGRUN"),
                           ("HUMANS_ZDF_W3_LOCO", "T_MAGWALK_2_MAGWALKL", "T_MAGWALKL_2_MAGWALK"),
                           ("HUMANS_ZDF_W3_SPRINT", "T_MAGRUN_2_MAGRUNL", "T_MAGRUNL_2_MAGRUN")):
        fs = _man_frames(os.path.join(make_loco.COMPILED, "%s-%s.MAN" % (o, start)))
        fe = _man_frames(os.path.join(make_loco.COMPILED, "%s-%s.MAN" % (o, stop)))
        assert max(_ang(fs[0][0][i], idle[0][i]) for i in body) < 25.0, (o, start)
        assert all(np.allclose(p, q, atol=1e-3) for p, q in zip(fe[-1][0], idle[0])), (o, stop)
    for o in ("HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_W3_SPRINT"):
        anis = {}
        for t, _, body_ in build_overlay.chunks(open(os.path.join(make_loco.COMPILED, o + ".MSB"), "rb").read()):
            if t == 0xF520:
                a = build_overlay.parse_ani(body_)
                anis[a["name"]] = a
        assert "S_MAGRUNL" in anis and not [n for n in anis if "CAST" in n or "SHOOT" in n], (o, sorted(anis))
        for n, a in anis.items():
            assert n in defs and a["layer"] == defs[n]["layer"] and a["next"] == defs[n]["next"], (o, n)

def test_loco_2h_transitions_join_parry_guard():
    # przejscia biegu / chodu z mieczem 2H stylu W3: z gardy parady do petli i z petli do gardy (ostatnia klatka =
    # garda) - kroki pozy na styku < 25 st., bez szarpniecia w trakcie (kosc do 60 st./kl.)
    import make_loco, make_moves
    guard = _guard("2H")
    if guard is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    guard = guard[0]
    for kind in ("SW_RUN", "SW_WALK", "SW_SPRINT"):
        model, spec = _loco_model(kind)
        if model is None:
            print("  (pominiety - brak zrzutow W3)")
            return
        loop = model.loop_frames()
        start = model.start_frames(guard, make_loco.START_FRAMES)
        stop = model.stop_frames(guard, make_loco.STOP_FRAMES)
        body = [SK.idx(n) for n in make_moves.JOIN_BONES]
        assert _same_pose(start[0], guard, 1e-6) and _same_pose(stop[-1], guard, 1e-6), kind
        # krok na styku nie wiekszy niz zwykly krok petli: sprint z bronia 600 cm/s (17.10, cykl 16 kl.) ma w samej petli
        # do ok. 35 st./kl. (lydka), trucht 24
        in_loop = max(max(_ang(loop[k][0][i], loop[(k + 1) % len(loop)][0][i]) for i in body) for k in range(len(loop)))
        lim = max(25.0, 1.05 * in_loop)
        assert max(_ang(start[-1][0][i], loop[0][0][i]) for i in body) < lim, (kind, lim)
        assert max(_ang(stop[0][0][i], loop[0][0][i]) for i in body) < lim, (kind, lim)
        for tag, seq in (("start", start + [loop[0]]), ("stop", [loop[-1]] + stop)):
            speed, _, _ = _motion_stats(seq, make_moves.JOIN_BONES)
            assert max(speed) < 60.0, (kind, tag, max(speed))


def _ramp_frames(n=51, deg=2.0):
    """Syntetyczny klip @25: kosc 0 obraca sie o deg st./kl. wokol Z, korzen idzie 1 cm/kl. w X."""
    out = []
    for f in range(n):
        a = np.radians(deg * f)
        R = np.array([[np.cos(a), -np.sin(a), 0.0], [np.sin(a), np.cos(a), 0.0], [0.0, 0.0, 1.0]])
        out.append(([R, np.eye(3)], np.array([float(f), 90.0, 0.0])))
    return out


def test_retime_windup_lands_hit_on_target_and_keeps_strike():
    # zamach mocnego ciecia W3 (cios w 17. kl. @25) przyspieszony tak, by cios wypadl w kl. 11: od ciosu klip bez zmian
    # (ta sama poza i predkosc), klatka 0 bez zmian, przyspieszenie gladkie (bez skoku predkosci przed ciosem) i ograniczone
    import make_attacks
    src = _ramp_frames()
    out, hit = make_attacks.retime_windup(src, 17, 11)
    assert hit == 11, hit
    assert len(out) == len(src) - 6, len(out)
    assert _ang(out[0][0][0], src[0][0][0]) < 1e-6
    for i in range(len(src) - 17):
        assert _ang(out[11 + i][0][0], src[17 + i][0][0]) < 0.05, i
        assert abs(out[11 + i][1][0] - src[17 + i][1][0]) < 0.05, i
    steps = [_ang(out[f][0][0], out[f + 1][0][0]) for f in range(len(out) - 1)]
    assert max(steps) <= 2.0 * make_attacks.WINDUP_KMAX + 1e-3, max(steps)
    assert all(b <= a + 1e-3 for a, b in zip(steps[:11], steps[1:12])), ("predkosc nie maleje gladko do ciosu", steps[:12])
    assert abs(steps[11] - 2.0) < 0.05 and steps[10] < 2.0 * 1.35, steps[9:13]
    same, h = make_attacks.retime_windup(src, 9, 11)   # cios juz wczesniej niz cel - bez zmian
    assert h == 9 and len(same) == len(src) and all(_ang(a[0][0], b[0][0]) < 1e-9 for a, b in zip(same, src))


def _style_dumps(w):
    """{klip: zrzut} kombinacji stylu w albo None bez zrzutow W3."""
    import make_attacks
    out = {}
    for c in make_attacks.STYLES[w]["combo"]:
        d = _move_dump(c)
        if d is None:
            return None
        out[c] = d
    return out


def test_w3_2h_combo_has_six_hits_at_gothic_tempo():
    # kombinacja 2H: 6 ciec (silnik czyta do 10 trafien z DEF_OPT_FRAME - oCAniCtrl_Human::GetFightLimbs 0x6AF3EF),
    # kazde inne, tempo porownywalne z 1H stylu W3 i zEBA 2HST3 (~1 s miedzy ciosami; natywne 2HST2 1,6-1,9 s):
    # 1. cios <= 0,52 s, odstep <= 26 kl. i najwyzej 15% dluzszy niz w 1H; sklejenia bez skokow pozy jak w 1H
    import make_attacks, make_pair
    d2, d1 = _style_dumps("2H"), _style_dumps("1H")
    g2, g1 = _guard("2H"), _guard("1H")
    if d2 is None or d1 is None or g2 is None or g1 is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    grip = make_pair.grip_local(VFS, SK)
    st2, st1 = make_attacks.STYLES["2H"], make_attacks.STYLES["1H"]
    assert st2["hits"] == 6 and st1["hits"] == 4
    frames, ev, _ = make_attacks.style_combo(d2, SK, grip, st2, g2[0])
    _, ev1, _ = make_attacks.style_combo(d1, SK, grip, st1, g1[0])
    opt, end, win = ev["opt"], ev["end"], ev["win"]
    assert len(opt) == 6 and len(end) == 6 and len(win) == 12 and len(set(ev["clips"])) == 6, ev
    assert opt[0] <= 13, opt
    gaps, gaps1 = np.diff(opt), np.diff(ev1["opt"])
    assert max(gaps) <= 26 and max(gaps) <= 1.15 * max(gaps1), (gaps, gaps1)
    prev_end = 0
    for k in range(6):
        assert prev_end < opt[k] < end[k] and opt[k] < win[2 * k] < win[2 * k + 1] <= end[k], (k, ev)
        prev_end = end[k]
    assert end[-1] == len(frames)
    body = [SK.idx(n) for n in ("BIP01", "BIP01 SPINE", "BIP01 SPINE1", "BIP01 SPINE2", "BIP01 L THIGH", "BIP01 R THIGH")]
    for f in end[:-1]:
        (la, ra), (lb, rb) = frames[f - 1], frames[f]
        assert np.linalg.norm(np.array(rb) - np.array(ra)) < 25.0, ("skok korzenia na sklejeniu", f)
        for i in body:
            assert _ang(la[i], lb[i]) < 25.0, ("skok pozy na sklejeniu", f, SK.names[i])


def test_w3_2h_single_attacks_hit_like_1h():
    # pojedyncze ciecia 2H (T_2HATTACKL/R) z gardy parady: cios najwyzej 2 kl. pozniej niz w 1H stylu W3 (dotad 8 kl.)
    import make_attacks, make_pair
    g2, g1 = _guard("2H"), _guard("1H")
    st2, st1 = make_attacks.STYLES["2H"], make_attacks.STYLES["1H"]
    dumps = [_move_dump(st2["L"]), _move_dump(st2["R"]), _move_dump(st1["L"])]
    if g2 is None or g1 is None or any(d is None for d in dumps):
        print("  (pominiety - brak zrzutow W3)")
        return
    grip = make_pair.grip_local(VFS, SK)
    _, hit1, _ = make_attacks.style_attack(dumps[2], SK, grip, st1, "L", g1[0])
    for d, side in zip(dumps[:2], "LR"):
        frames, hit, _ = make_attacks.style_attack(d, SK, grip, st2, side, g2[0])
        assert hit <= hit1 + 2, (side, hit, hit1)
        assert len(frames) <= 34, (side, len(frames))


def _msb_attack_events(path):
    """{nazwa animacji: {"frames": n, "opt": [...], "end": [...], "win": [...]}} z MSB nakladki."""
    import build_overlay
    out, cur = {}, None
    for t, _, body in build_overlay.chunks(open(path, "rb").read()):
        if t == 0xF520:
            a = build_overlay.parse_ani(body)
            cur = out.setdefault(a["name"], {"frames": a["last"] - a["first"] + 1, "fps": a["fps"]})
        elif cur is not None and t == 0xF5A3:
            lines = body[4:].decode("latin1").split("\n")
            key = {"DEF_OPT_FRAME": "opt", "DEF_HIT_END": "end", "DEF_WINDOW": "win"}.get(lines[0])
            if key:
                cur[key] = [int(x) for x in lines[1].split()]
    return out


def test_swing_sweep_matches_native_directions():
    # kalibracja kierunku ciecia: natywne ciecia boczne HUMANS, talentu 1HST2 i 2HST2 - T_xHATTACKL = zamach z prawej
    # w lewo (azymut czubka maleje), R = z lewej w prawo
    import make_attacks
    for ovl, ani, hit in (("HUMANS", "T_1HATTACKL", 5), ("HUMANS", "T_1HATTACKR", 5), ("HUMANS", "T_2HATTACKL", 6),
                          ("HUMANS", "T_2HATTACKR", 6), ("HUMANS_1HST2", "T_1HATTACKL", 3), ("HUMANS_1HST2", "T_1HATTACKR", 3),
                          ("HUMANS_2HST2", "T_2HATTACKL", 5), ("HUMANS_2HST2", "T_2HATTACKR", 5)):
        m = manlib.Man(bytes(VFS.find("%s-%s.MAN" % (ovl, ani)).data))
        frames = [_sign_pose(m, k) for k in range(m.frames)]
        s = make_attacks.swing_sweep(frames, SK, hit - 1)
        assert abs(s) > 60.0 and (s < 0) == ani.endswith("L"), (ovl, ani, s)


def test_w3_2h_side_alt_is_faster_two_handed_set():
    # "W3 szybkie" ciecia boczne 2H: lekkie ciecia NPC z mieczem dwurecznym W3, cios wczesniej niz mocne ciecia Geralta
    import make_attacks
    alt = make_attacks.SIDE_ALTS["2HS"]
    assert alt["style"] == "2H" and alt["overlay"] == "HUMANS_ZDF_W3_2HS"
    assert alt["set"].lower().endswith("man_sword_2handed.w2anims")
    assert alt["windup"] < make_attacks.STYLES["2H"]["windup"]
    assert alt["L"] != alt["R"]


def test_w3_2h_side_alt_files_hit_early_in_right_direction():
    # po make_attacks.py --only 2HS: nakladka HUMANS_ZDF_W3_2HS ma tylko T_2HATTACKL/R (plugin wstawia je w miejsce ciec
    # bocznych HUMANS_ZDF_W3_2H), cios w kl. windup+1 (<= 0,32 s), okno kolejnego ciosu 3 kl. po nim (szybciej niz
    # mocne ciecia W3), L z prawej w lewo, R z lewej w prawo, start i koniec w gardzie parady 2H
    import make_attacks
    alt = make_attacks.SIDE_ALTS["2HS"]
    msb = os.path.join(make_attacks.COMPILED, alt["overlay"] + ".MSB")
    guard2 = _guard("2H")
    if guard2 is None or not os.path.exists(msb):
        print("  (pominiety - brak nakladki, uruchom make_attacks.py --only 2HS)")
        return
    ev = _msb_attack_events(msb)
    assert sorted(ev) == ["T_2HATTACKL", "T_2HATTACKR"], sorted(ev)
    w3 = _msb_attack_events(os.path.join(make_attacks.COMPILED, "HUMANS_ZDF_W3_2H.MSB"))
    names = ("BIP01", "BIP01 PELVIS", "BIP01 SPINE2", "BIP01 HEAD", "BIP01 R HAND", "BIP01 L HAND", "ZS_RIGHTHAND")

    def pts(pose):
        W = SK.compose(pose[0], np.array([0.0, pose[1][1], 0.0]))
        return np.array([W[SK.idx(n)][:3, 3] for n in names])

    for ani, e in ev.items():
        opt, win = e["opt"][0], e["win"]
        assert opt == alt["windup"] + 1 and opt / 25.0 <= 0.32 + 1e-6, (ani, opt)
        assert win[0] == opt + 3 and win[1] == e["frames"] == e["end"][0], (ani, e)
        assert win[0] < w3[ani]["win"][0] and opt < w3[ani]["opt"][0], (ani, e, w3[ani])
        fr = _man_frames(os.path.join(make_attacks.COMPILED, "%s-%s.MAN" % (alt["overlay"], ani)))
        assert len(fr) == e["frames"]
        s = make_attacks.swing_sweep(fr, SK, opt - 1)
        assert abs(s) > 60.0 and (s < 0) == ani.endswith("L"), (ani, s)
        for tag, pose in (("start", fr[0]), ("koniec", fr[-1])):
            d = float(np.abs(pts(pose) - pts(guard2[0])).max())
            assert d < 1.5, (ani, tag, d)


def test_traversal_sections_pick_clips_and_landings():
    # przeskok: klip + ladowanie w bieg (30 cm konczy sie w biegu, 250 cm bez wlasnego ladowania - ogolne), wspiecie: klip
    import make_traversal as T
    names = set(T.CLIPS)
    assert T.section_clips("CLIMB", 100, "lf", names) == ["climb_run_100_lf"]
    assert T.section_clips("VAULT", 100, "rf", names) == ["vault_run_100_rf", "vault_run_100_rf_land_to_run"]
    assert T.section_clips("VAULT", 30, "lf", names) == ["vault_run_30_lf"]
    assert T.section_clips("VAULT", 250, "lf", names) == ["vault_run_250_lf", None]
    assert T.section_clips("CLIMB", 300, "lf", names) is None
    assert len(T.all_sections(names)) == 32 and T.section_id("VAULT", 150, "rf") == "VAULT_150_RF"
    # z miejsca: climb_idle_<h> i vault_idle_<h> (30-300; przeskok bez doklejonego ladowania - skok miednicy na sklejeniu)
    assert T.section_id("CLIMB", 250, T.IDLE) == "CLIMB_IDLE_250" and T.section_clips("CLIMB", 300, T.IDLE, names) == ["climb_idle_300"]
    assert T.section_clips("VAULT", 100, T.IDLE, names) == ["vault_idle_100"]
    assert sum(1 for s in T.all_sections(names) if s[2] == T.IDLE) == 12
    assert T.support_level(10.0, "CLIMB", 100.0, 50.0, 0.0) == 0.0 and T.support_level(80.0, "CLIMB", 100.0, 50.0, 0.0) == 100.0
    assert T.support_level(80.0, "VAULT", 100.0, 50.0, 40.0) == 100.0 and T.support_level(120.0, "VAULT", 100.0, 50.0, 40.0) == 0.0


def test_traversal_vob_path_carries_vertical_motion_from_ground():
    # vob startuje z ziemi (przesuniecie miednicy klipu wygasa w ENTRY_FADE), potem niesie cale pionowe przemieszczenie
    # miednicy (korzen animacji stoi na P0), x/z jak korzen
    import make_traversal as T
    p0 = 98.0
    frames = [(None, np.array([0.0, 92.0 + 4.0 * k, 10.0 * k])) for k in range(20)]
    path = T.vob_path(frames, p0)
    assert path[0] == (0.0, 0.0, 0.0)
    fade_k = int(T.ENTRY_FADE * T.FPS) + 1
    for k in range(fade_k, 20):
        assert abs(path[k][1] - (92.0 + 4.0 * k - p0)) < 1e-6 and abs(path[k][2] - 10.0 * k) < 1e-6
    assert all(b[1] >= a[1] - 1e-6 for a, b in zip(path, path[1:]))   # bez skoku w dol przy wygasaniu


def _trav_ini():
    import make_traversal as T
    if not os.path.exists(T.INI):
        return None
    text = open(T.INI, encoding="ascii").read()
    out = {}
    for sec in text.split("\n[")[1:]:
        sid, body = sec.split("]\n", 1)
        out[sid] = dict(l.split("=", 1) for l in body.splitlines() if "=" in l and not l.startswith(";"))
    return out


def test_traversal_analyze_climb_and_vault_geometry():
    # punkty klipow W3 (zrzuty w anims\work\w3trav): odbicie przed podparciem, sciana miedzy nogami a podparciem,
    # wspiecie konczy sie stojac na gorze, przeskok laduje za przeszkoda po szczycie
    import make_traversal as T, make_pair
    if not os.path.exists(os.path.join(T.WORK, "climb_run_100_lf.json")) or not os.path.exists(os.path.join(T.WORK, "vault_run_150_lf.json")):
        print("  (pominiety - brak zrzutow W3, uruchom make_traversal.py)")
        return
    grip = make_pair.grip_local(VFS, SK)
    frames, k, d = T.load_motion(["climb_run_100_lf"], SK, grip)
    top = T.clip_top(d, "CLIMB", 100, k)
    a = T.analyze(frames, SK, "CLIMB", top)
    assert 95.0 < top < 105.0, top
    assert a["takeoff"] < a["contact"] <= a["exit"] <= a["settle"] < len(frames), a
    assert a["face_lo"] <= a["edge"] <= a["face_hi"] and 100.0 < a["edge"] < 200.0, a
    root = T.node_track(frames[:a["settle"] + 1], SK, ("BIP01",))["BIP01"]
    assert root[-1][1] > top + 80.0 and root[-1][2] > a["edge"] + 20.0   # stoi na gorze za krawedzia
    frames, k, d = T.load_motion(["vault_run_150_lf", "vault_run_150_lf_land_to_run"], SK, grip)
    top = T.clip_top(d, "VAULT", 150, k)
    a = T.analyze(frames, SK, "VAULT", top)
    assert a["contact"] <= a["apex"] <= a["release"] < a["land"] <= a["exit"], a
    assert 15.0 <= a["thick"] <= T.VAULT_THICK_MAX and a["support"] in T.HAND_NODES, a


def test_traversal_climb_depth_counts_only_until_game_takes_over():
    # wymagana glebokosc gory: wspiecie 30 cm konczy sie w biegu - liczy sie droga do exit (dalej bieg gry), wyzsze do
    # konca wstawania; promien ciala 20 cm (plugin doklada 20 cm tolerancji) - realne skrzynie 60-100 cm maja klip
    import make_traversal as T
    ini = _trav_ini()
    if not ini:
        print("  (pominiety - brak traversal_w3.ini, uruchom make_traversal.py)")
        return
    assert T.BODY_RADIUS <= 20.0
    for sid, sec in ini.items():
        if sid.startswith("CLIMB_"):
            d = float(sec["depth"])
            assert 40.0 <= d <= (100.0 if sid.startswith("CLIMB_30_") else 105.0), (sid, d)


def test_traversal_overlays_fly_in_place_and_end_in_stance():
    # po make_traversal.py: 4 animacje na sekcje (bez broni, piesci, 1H, 2H) z flagami MF, korzen w miejscu na stalej
    # wysokosci postawy (po ENTRY_FADE), ostatnia klatka = postawa broni; tor konczy sie na gorze (wspiecie) / na ziemi
    import make_traversal as T, recovery
    ini = _trav_ini()
    if not ini:
        print("  (pominiety - brak traversal_w3.ini, uruchom make_traversal.py)")
        return
    assert len(ini) == 32, sorted(ini)
    assert all((sec.get("start") == "IDLE") == ("_IDLE_" in sid) for sid, sec in ini.items())
    stances = {st: recovery.stance_pose(VFS, SK, st) for _, st in T.ANIS}
    p0 = float(stances["S_RUN"][1][1])
    for sid in ("CLIMB_100_LF", "VAULT_150_RF", "CLIMB_30_RF", "CLIMB_IDLE_250", "VAULT_IDLE_100"):
        sec = ini[sid]
        anis = read_msb_anis(os.path.join(T.COMPILED, sec["overlay"] + ".MSB"))
        pts = [tuple(float(v) for v in p.split(",")) for p in sec["traj"].split()]
        end_y = float(sec["height"]) if sid.startswith("CLIMB") else 0.0
        assert abs(pts[-1][1] - end_y) < 1.5 and pts[0] == (0.0, 0.0, 0.0), (sid, pts[-1])
        assert max(abs(b[1] - a[1]) + abs(b[2] - a[2]) for a, b in zip(pts, pts[1:])) < 45.0, sid   # bez skokow toru
        assert abs(float(sec["cancel"]) * T.FPS - len(pts)) < 0.6, sid
        fade = int(T.ENTRY_FADE * T.FPS) + 1
        for ani, st in T.ANIS:
            assert anis[ani].next == st, (sid, ani, anis[ani].next)
            m = manlib.Man(open(os.path.join(T.COMPILED, "%s-%s.MAN" % (sec["overlay"], ani)), "rb").read())
            assert m.frames == len(pts), (sid, ani, m.frames, len(pts))
            frames = [_sign_pose(m, k) for k in range(m.frames)]
            for l, r in frames:
                assert abs(r[0]) < 0.5 and abs(r[2]) < 0.5, (sid, ani, r)
            for l, r in frames[fade:-T.REC_FRAMES]:
                assert abs(r[1] - p0) < 0.5, (sid, ani, r[1], p0)
            t_local, t_root = stances[st]
            W1 = SK.compose(frames[-1][0], np.zeros(3))
            W2 = SK.compose(t_local, np.zeros(3))
            for n in ("BIP01 HEAD", "BIP01 R HAND", "BIP01 L FOOT"):
                assert np.linalg.norm(W1[SK.idx(n)][:3, 3] - W2[SK.idx(n)][:3, 3]) < 1.5, (sid, ani, n)
            assert abs(frames[-1][1][1] - float(t_root[1])) < 0.5, (sid, ani)
        raw = open(os.path.join(T.COMPILED, sec["overlay"] + ".MSB"), "rb").read()
        assert raw.count(b"MF\nZDISM_W3.ASC\n") == len(T.ANIS), sid   # flaga F (lot) w kazdej animacji


# ---------- Krotsze uniki i przewroty, okno wyjscia (17.10) ----------

def _synthetic_motion(n=21):
    """Obrot korzenia wokol Y ze zmienna predkoscia (szybko na poczatku, wolno na koncu) i ruch do przodu."""
    import make_moves
    out, ang, z = [], 0.0, 0.0
    for k in range(n):
        local = [l[:3, :3].copy() for l in SK.local]
        local[0] = make_moves.expmap(np.array([0.0, np.radians(ang), 0.0]))
        out.append((local, np.array([0.0, float(SK.root_pos[1]), z])))
        step = 12.0 if k < n // 2 else 3.0
        ang += step
        z += step * 2.0
    return out


def test_retime_keeps_endpoints_path_and_length():
    # przyspieszenie koncowki: dokladnie n_out klatek, pierwsza i ostatnia klatka jak w zrodle (ta sama droga i poza
    # koncowa), poczatek (przed 'start') bez zmian, wolna koncowka przyspieszona bardziej niz szybki poczatek
    import make_moves
    src = _synthetic_motion()
    out = make_moves.retime(src, SK, 15, start=2)
    assert len(out) == 15
    assert _same_pose(out[0], src[0], 1e-9) and _same_pose(out[-1], src[-1], 1e-6)
    assert np.allclose(out[-1][1], src[-1][1], atol=1e-6) and np.allclose(out[0][1], src[0][1], atol=1e-9)
    assert all(_same_pose(a, b, 1e-6) for a, b in zip(out[:2], src[:2]))
    step = lambda fr, k: float(fr[k + 1][1][2] - fr[k][1][2])
    # ruch do przodu caly czas w tym samym kierunku (bez cofania w czasie)
    assert all(step(out, k) > 0.0 for k in range(len(out) - 1))
    # tempo koncowki (cm/kl.) wzroslo wyraznie bardziej niz poczatku
    assert step(out, len(out) - 2) / 6.0 > 1.4 * step(out, 2) / 24.0, ([step(out, k) for k in range(len(out) - 1)])


def test_retime_without_shortening_is_identity():
    import make_moves
    src = _synthetic_motion()
    out = make_moves.retime(src, SK, len(src), start=2)
    assert len(out) == len(src) and all(_same_pose(a, b, 1e-6) and np.allclose(a[1], b[1], atol=1e-6) for a, b in zip(out, src))


def _evade_moves(grip):
    """Uniki i przewroty jak w make_moves.main (z przycieciem do EVADE_LENGTH): [(id, wynik build_move_full, zrodlo bez
    przyciecia)]."""
    import make_moves
    out = []
    for k, c in make_moves.DODGES.items():
        d = _move_dump(c)
        if d is None:
            return None
        out.append(("DODGE_" + k, make_moves.build_move_full(d, SK, grip, 1.0, 0.5, length=make_moves.evade_length("DODGE_" + k)),
                    make_moves.build_move_full(d, SK, grip, 1.0, 0.5)))
    dumps = [_move_dump(c) for c in make_moves.ROLL]
    if any(x is None for x in dumps):
        return None
    roll = make_moves.concat(dumps)
    for k, y in make_moves.ROLL_YAW.items():
        out.append(("ROLL_" + k, make_moves.build_move_full(roll, SK, grip, 1.0, make_moves.ROLL_IFRAMES, y, make_moves.ROLL_CANCEL,
                                                            length=make_moves.evade_length("ROLL_" + k)),
                    make_moves.build_move_full(roll, SK, grip, 1.0, make_moves.ROLL_IFRAMES, y, make_moves.ROLL_CANCEL)))
    d = _move_dump(make_moves.ROLL_B_CLIP)
    if d is not None:
        out.append(("ROLL_B", make_moves.build_roll_back(d, SK, grip, length=make_moves.evade_length("ROLL_B")),
                    make_moves.build_roll_back(d, SK, grip)))
    return out


def test_evade_moves_shorter_same_path_and_exit_window():
    # decyzja autora 17.10: unik ok. 0,85-0,9 s (bylo do 1,2 s), przewrot ok. 1,0 s (bylo 1,44 s), ta sama droga;
    # okno wyjscia (atak / blok / kolejny ruch z bufora) po fazie przemieszczenia, gdy stopa stoi na ziemi
    import make_moves, make_pair
    moves = _evade_moves(make_pair.grip_local(VFS, SK))
    if moves is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    for mid, (frames, motion, traj, iframes, cancel), (_, _, traj0, _, cancel0) in moves:
        limit = 0.92 if mid.startswith("DODGE") else (1.05 if mid != "ROLL_B" else 1.3)
        assert cancel <= limit + 1e-6 and cancel <= cancel0 + 1e-6, (mid, cancel, cancel0)
        assert abs(cancel - (len(frames) + make_moves.REC_FRAMES) / 25.0) < 1e-6, (mid, cancel, len(frames))
        d, d0 = np.hypot(*traj[-1]), np.hypot(*traj0[-1])
        assert abs(d - d0) < 0.01 * d0 + 1.0, (mid, "droga", d, d0)
        assert 0.0 < iframes[0] < iframes[1] < cancel, (mid, iframes)
        ex = make_moves.evade_exit(motion, traj, iframes, SK, make_moves.exit_share(mid))
        k = int(round(ex * 25))
        assert iframes[1] - 1e-6 <= ex <= cancel - 0.2, (mid, ex, iframes, cancel)
        assert make_moves.foot_down(motion[k], SK), (mid, "stopa w powietrzu w oknie wyjscia", ex)
        print("   %s: %.2f s (bylo %.2f), droga %.0f cm, nietykalnosc do %.2f, wyjscie od %.2f s" % (mid, cancel, cancel0, d,
              iframes[1], ex))


def test_evade_retime_has_no_pose_pops():
    # przyspieszona koncowka najwyzej troche szybsza niz najszybsze miejsce samego klipu W3 - bez przeskokow pozy.
    # Przewrot 1,0 s przy tej samej drodze (509 cm) wymaga sredniej ok. 7 m/s, a klip W3 ma szczyt 6,6 m/s - korzen do
    # 1,35x szczytu klipu, kosci do 1,2x (przewrot obraca cialo ok. 80 st./kl. juz w klipie; przeskok pozy to > 135)
    import make_moves, make_pair
    moves = _evade_moves(make_pair.grip_local(VFS, SK))
    if moves is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    for mid, (_, motion, _, _, _), (_, motion0, _, _, _) in moves:
        sp, acc, racc = _motion_stats(motion, make_moves.JOIN_BONES)
        sp0, acc0, racc0 = _motion_stats(motion0, make_moves.JOIN_BONES)
        assert max(sp) <= 1.2 * max(sp0) and max(sp) < 135.0, (mid, "kosci", max(sp), max(sp0))
        assert max(acc) <= 1.6 * max(acc0) + 2.0, (mid, "przyspieszenie kosci", max(acc), max(acc0))
        assert max(racc) <= 1.8 * max(racc0) + 2.0, (mid, "przyspieszenie korzenia", max(racc), max(racc0))
        v = lambda m: max(np.hypot(m[k + 1][1][0] - m[k][1][0], m[k + 1][1][2] - m[k][1][2]) for k in range(len(m) - 1))
        assert v(motion) <= 1.35 * v(motion0) + 1.0, (mid, "korzen", v(motion), v(motion0))
        print("   %s: kosci %.0f / %.0f st./kl., korzen %.0f / %.0f cm/s, przysp. kosci %.0f / %.0f, korzenia %.1f / %.1f" % (
            mid, max(sp), max(sp0), v(motion) * 25, v(motion0) * 25, max(acc), max(acc0), max(racc), max(racc0)))


def test_moves_ini_evade_exit_keys():
    # po make_moves.py: uniki i przewroty z exit= (okno wyjscia) w granicach ruchu, krotsze cancel=
    import make_moves
    if not os.path.exists(make_moves.INI):
        print("  (pominiety - brak moves_w3.ini)")
        return
    text = open(make_moves.INI, encoding="ascii").read()
    for mid in ["DODGE_" + k for k in make_moves.DODGES] + ["ROLL_" + k for k in make_moves.ROLL_YAW] + ["ROLL_B"]:
        sec = dict(l.split("=", 1) for l in text.split("[%s]" % mid, 1)[1].split("\n[", 1)[0].splitlines() if "=" in l and
                   not l.startswith(";"))
        assert "exit" in sec, mid
        ex, cancel = float(sec["exit"]), float(sec["cancel"])
        it = [float(x) for x in sec["iframes"].split(",")]
        assert it[1] <= ex <= cancel - 0.2, (mid, ex, cancel)
        assert cancel <= (0.92 if mid.startswith("DODGE") else 1.3) + 1e-6, (mid, cancel)


def test_loco_sword_sprint_is_600_and_planted():
    # decyzja autora 17.10: sprint z bronia 600 cm/s (bylo 474) - mieszanka szybkiego biegu i sprintu W3 z mieczem,
    # petla bez szwu, stopa w podporze stoi
    model, spec = _loco_model("SW_SPRINT")
    if model is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    assert spec["speed"] == 600.0 and "sprint" in spec["fast"], spec
    frames = model.loop_frames()
    n = len(frames)
    z = np.array([f[1][2] for f in frames])
    speed = np.polyfit(np.arange(n), z, 1)[0] * retarget.FPS_OUT
    assert abs(speed - 600.0) / 600.0 < 0.03, speed
    steps = [max(_ang(x, y) for x, y in zip(frames[k][0], frames[(k + 1) % n][0])) for k in range(n)]
    assert steps[-1] < 1.5 * max(steps[:-1]), (steps[-1], max(steps[:-1]))
    for foot in ("BIP01 L FOOT", "BIP01 R FOOT"):
        p = [SK.compose(l, r)[SK.idx(foot)][:3, 3] for l, r in frames]
        v = [np.linalg.norm(p[k + 1] - p[k]) * retarget.FPS_OUT for k in range(len(p) - 1)]
        assert min(v) < 0.15 * 600.0, (foot, min(v))
    # miecz trzymany przy biodrze jak w biegu z bronia (nie wymachiwany jak w sprincie bez broni)
    hand = [SK.compose(l, r)[SK.idx("ZS_RIGHTHAND")][:3, 3] - r for l, r in frames]
    assert max(h[1] for h in hand) - min(h[1] for h in hand) < 30.0, [round(float(h[1])) for h in hand]


def _pair():
    hero = os.path.join(WORK, "w3", "hero_01_rp.json")
    victim = os.path.join(WORK, "w3", "victim_01_rp.json")
    if not (os.path.exists(hero) and os.path.exists(victim)):
        return None
    import make_pair
    return make_pair.build_pair(json.load(open(hero)), json.load(open(victim)), SK, VFS)


def test_pair_hero_starts_at_origin_facing_forward():
    p = _pair()
    if p is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    assert abs(p["heroTraj"][0][0]) < 0.5 and abs(p["heroTraj"][0][1]) < 0.5, p["heroTraj"][0]
    vx, vz = p["victimTraj"][0]
    assert 50.0 < (vx * vx + vz * vz) ** 0.5 < 250.0, (vx, vz)


def test_pair_victim_stands_in_front_facing_hero():
    # W3 ustawia pare przez kosc Reference: u kazdej postaci to miejsce drugiej (ok. 1,6 m przed nia, twarza w twarz)
    import make_pair
    p = _pair()
    if p is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    vx, vz = p["victimTraj"][0]
    assert 100.0 < vz < 250.0 and abs(vx) < 0.25 * vz, ("ofiara nie stoi przed bohaterem", vx, vz)
    # animacja ofiary w ukladzie jej voba (obroconego twarza do bohatera): cialo patrzy w +Z
    vyaw = make_pair.body_yaw(p["victimInplace"][0][0][0], SK)
    assert abs(vyaw) < np.radians(30), ("ofiara nie patrzy na bohatera", np.degrees(vyaw))


def test_pair_remaster_uses_shared_scene_space():
    # klipy *_remaster: Reference = Trajectory, obie postacie w jednej przestrzeni sceny - ofiara nie moze trafic w bohatera
    import make_pair
    clip = "man_finisher_sword_v3_lp_remaster"
    hero = os.path.join(WORK, "w3", "hero_%s.json" % clip)
    victim = os.path.join(WORK, "w3", "victim_%s.json" % clip)
    if not (os.path.exists(hero) and os.path.exists(victim)):
        print("  (pominiety - brak zrzutow W3)")
        return
    p = make_pair.build_pair(json.load(open(hero)), json.load(open(victim)), SK, VFS)
    vx, vz = p["victimTraj"][0]
    assert 80.0 < vz < 250.0 and abs(vx) < 0.25 * vz, ("ofiara nie stoi przed bohaterem", vx, vz)


def test_pair_blade_reaches_neck_at_cut():
    p = _pair()
    if p is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    assert p["cutDist"] < 25.0, ("ostrze mija szyje", p["cutDist"], p["cutFrame"])


def test_pair_keeps_longer_victim_animation():
    # DLC: ofiara pada dluzej niz trwa animacja bohatera (glowa 4,7 s vs 2,8 s) - nie ucinac jej upadku
    import make_pair
    hero = os.path.join(WORK, "w3", "hero_01_rp.json")
    victim = os.path.join(WORK, "w3", "victim_01_rp.json")
    if not (os.path.exists(hero) and os.path.exists(victim)):
        print("  (pominiety - brak zrzutow W3)")
        return
    dh, dv = json.load(open(hero)), json.load(open(victim))
    dh["frames"] = dh["frames"][:60]
    p = make_pair.build_pair(dh, dv, SK, VFS)
    assert len(p["victim"]) > len(p["hero"]) + 10, (len(p["hero"]), len(p["victim"]))
    assert p["cutFrame"] < len(p["hero"])


def test_slice_pair_starts_later_for_dazed_victim():
    # wariant egzekucji oszolomionego: para od klatki, w ktorej ofiara juz kleczy; tory od nowa od bohatera
    import make_pair
    p = _pair()
    if p is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    s = 20
    q = make_pair.slice_pair(p, s)
    assert q["cutFrame"] == p["cutFrame"] - s
    assert q["heroTraj"][0] == (0.0, 0.0)
    hx, hz = p["heroTraj"][s]
    vx, vz = p["victimTraj"][s]
    assert abs(q["victimTraj"][0][0] - (vx - hx)) < 1e-4 and abs(q["victimTraj"][0][1] - (vz - hz)) < 1e-4
    assert len(q["hero"]) == len(p["hero"]) - s and len(q["victim"]) == len(p["victim"]) - s
    assert len(q["victimA"]) + len(q["victimB"]) - 1 == len(q["victim"])
    assert np.allclose(q["victimA"][0][0][0], p["victimInplace"][s][0][0])


def test_pair_inplace_and_split_are_consistent():
    p = _pair()
    if p is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    for local, root in p["heroInplace"] + p["victimInplace"]:
        assert abs(root[0]) < 1e-6 and abs(root[2]) < 1e-6
    c = p["cutFrame"]
    a, b = p["victimA"], p["victimB"]
    assert 0 < c < len(p["victim"]) - 1
    assert len(a) + len(b) - 1 == len(p["victim"])
    assert np.allclose(a[-1][0][0], b[0][0][0]) and np.allclose(a[-1][1], b[0][1])
    assert len(p["heroTraj"]) == len(p["hero"]) and len(p["victimTraj"]) == len(p["victim"])


def read_msb_anis(path):
    s = zenkit.ModelScript.load(path)
    return {a.name: a for a in s.animations}


def test_daze_overlay_maps_wounded_to_kneel():
    import build_daze, build_overlay
    with tempfile.TemporaryDirectory() as tmp:
        build_daze.build(tmp, VFS)
        anis = read_msb_anis(os.path.join(tmp, "HUMANS_ZDF_DAZE.MSB"))
        for name in ("T_STAND_2_WOUNDED", "T_STAND_2_WOUNDEDB", "S_WOUNDED", "S_WOUNDEDB", "T_WOUNDED_2_DEAD", "T_WOUNDEDB_2_DEADB"):
            assert name in anis, name
            m = manlib.Man(open(os.path.join(tmp, "HUMANS_ZDF_DAZE-%s.MAN" % name), "rb").read())
            assert m.name == name and m.frames == anis[name].last_frame - anis[name].first_frame + 1, name
        assert anis["S_WOUNDED"].next == "S_WOUNDED"
        assert anis["T_STAND_2_WOUNDED"].next == "S_WOUNDED"
        # smierc z kleczek: tresc i next z gry (lezenie), dlugie przejscie - cialo osuwa sie z kleczek zamiast wstawac
        defs = build_overlay.humans_ani_defs(VFS)
        for name in ("T_WOUNDED_2_DEAD", "T_WOUNDEDB_2_DEADB"):
            assert anis[name].next == defs[name]["next"], name
            assert anis[name].blend_in >= 0.3, (name, anis[name].blend_in)


def test_overlay_next_is_fight_stance():
    import build_overlay
    with tempfile.TemporaryDirectory() as tmp:
        src = make_2h_man(tmp)
        os.rename(src, os.path.join(tmp, "HUMANS_ZDF_T-T_2HSFINISH.MAN"))
        build_overlay.build("HUMANS_ZDF_T", tmp, next_ani="S_2HRUN", vfs=VFS)
        anis = read_msb_anis(os.path.join(tmp, "HUMANS_ZDF_T.MSB"))
        assert anis["T_2HSFINISH"].next == "S_2HRUN"


def test_vdf_roundtrip_with_nested_dirs():
    import build_vdf
    files = {
        "SYSTEM\\AUTORUN\\ZDISMEMBER.DLL": b"MZ" + bytes(range(200)),
        "SYSTEM\\ZDISMEMBER\\FINISHERS.INI": b"[FIN]\nweapon=2H\n",
        "_WORK\\DATA\\ANIMS\\_COMPILED\\HUMANS_ZDF_A.MSB": b"msb-a",
        "_WORK\\DATA\\ANIMS\\_COMPILED\\HUMANS_ZDF_A-T_2HSFINISH.MAN": b"man" * 50,
    }
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "t.vdf")
        build_vdf.write_vdf(path, files, "test")
        v = zenkit.Vfs()
        v.mount_disk(path, clobber=zenkit.VfsOverwriteBehavior.NEWER)
        for name, data in files.items():
            node = v.find(name.split("\\")[-1])
            assert node is not None, name
            assert bytes(node.data) == data, name
        del v


def test_vdf_nb_extra_scripts_only_in_nb_pack():
    # runda 6: data_nb (premia kieszonkowa Axii - RX_GetThieftChance / B_Addon_Steal NB) tylko w paczce --nb
    import build_vdf
    thief = "SYSTEM\\AUTORUN\\ZDISMEMBER_NB_THIEF.D"
    steam = build_vdf.collect(with_dll=False)
    assert thief not in steam
    nb = build_vdf.nb_files(build_vdf.collect(with_dll=False))
    assert thief in nb
    text = nb[thief].decode("utf-8")
    assert not text.startswith("﻿")
    assert "func int RX_GetThieftChance(var C_NPC thief, var C_NPC target)" in text
    assert "ZDF_AxiiThiefBonus(target)" in text
    assert "_old(" not in text.lower()   # bez hakow _Old (w NB funkcja int z parametrami C_NPC zwraca przez nie 0)
    assert "var int ZDF_AxiiThiefId[8];" in text and "var int ZDF_AxiiThiefPct[8];" in text
    text.encode("cp1250")   # tekst do gry w cp1250


_MEDITATE = {}


def _meditate_chain():
    """(start, petla, wstawanie, stand, body, floor) z make_meditate.build_chain albo None bez zrzutow W3."""
    import make_meditate as mm, make_pair, make_signs
    if "chain" not in _MEDITATE:
        paths = [os.path.join(mm.WORK, c + ".json") for c in mm.VARIANTS[1]]
        if not all(os.path.exists(p) for p in paths):
            _MEDITATE["chain"] = None
        else:
            grip = make_pair.grip_local(VFS, SK)
            stand = make_signs.mean_pose(VFS, SK, mm.STAND)
            body = mm.Body(VFS, SK)
            chain = mm.build_chain(VFS, SK, grip, stand, [json.load(open(p)) for p in paths])
            _MEDITATE["chain"] = chain + (stand, body, body.lowest(stand))
    return _MEDITATE["chain"]


def test_w3_meditation_chain_is_continuous():
    # medytacja (make_meditate): stoi (S_RUN Gothica) -> kleczy -> petla -> wstaje do S_RUN; styki bez skokow,
    # petla domknieta, siad na pietach W3 (24.10: korzen obnizony do podparcia na podlozu ~21 cm), stopy nie pod ziemia
    import make_meditate as mm
    ch = _meditate_chain()
    if ch is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    start, loop, stop, stand = ch[:4]
    assert _same_pose(start[-1], loop[0], 1e-6) and _same_pose(stop[-1], stand, 1e-6)
    assert np.allclose(start[0][1], stand[1], atol=15.0) and np.allclose(stop[0][1], loop[0][1], atol=1e-6)
    W = lambda f: SK.compose(*f)
    worst = max(np.linalg.norm(W(a)[i][:3, 3] - W(b)[i][:3, 3])
                for a, b in zip(loop, loop[1:] + loop[:1]) for i in range(len(SK.names)))
    assert worst < 2.0, ("szew petli", worst)
    assert 15.0 < loop[0][1][1] < 30.0, loop[0][1]
    legs = [SK.idx(n) for n in ("BIP01 L FOOT", "BIP01 R FOOT", "BIP01 L TOE0", "BIP01 R TOE0")]
    low = min(W(f)[i][1, 3] for f in start + loop + stop for i in legs)
    assert low > -5.0, low
    print("   medytacja: kleczenie korzen %.1f cm, najnizsza stopa %.1f cm, szew petli %.2f cm" % (loop[0][1][1], low, worst))


def test_w3_meditation_feet_apart_and_on_floor():
    # zgloszenie 24.10: "nogi sie merguja w stopach" + postac unosi sie nad ziemia. Retarget samych obrotow na nogi
    # Gothica (udo 39 / lydka 49 / stopa 26 cm wobec W3 49 / 44 / 18) sciagal kostki do srodka (osie stop 3,6 cm od
    # siebie przy stopie siatki 14,6 cm) i zostawial klek ~13 cm nad podlozem. Kazda klatka trzech klipow:
    #  - osie stop i lydek L-R >= FEET_MIN, siatki stop rozdzielone plaszczyzna (szukana po 600 kierunkach) z zapasem,
    #  - stopa nie w udzie (os stopy >= 13 cm od osi ud),
    #  - podloze = najnizszy wierzcholek siatki ciala w S_RUN: w petli kolana i stopy obu nog 0-1,5 cm nad nim
    #    (najnizszy punkt +-1 cm), w przejsciach chwilowo najwyzej FLOOR_PEN pod / 3 cm nad, korzen bez skokow.
    import make_meditate as mm
    ch = _meditate_chain()
    if ch is None:
        print("  (pominiety - brak zrzutow W3)")
        return
    start, loop, stop, stand, body, floor = ch
    foot_ids = {s: [SK.idx("BIP01 %s FOOT" % s), SK.idx("BIP01 %s TOE0" % s)] for s in "LR"}
    dirs = np.random.default_rng(1).normal(size=(600, 3))
    dirs /= np.linalg.norm(dirs, axis=1)[:, None]
    worst_gap, worst_sep, worst_thigh = 1e9, 1e9, 1e9
    for name, clip in (("start", start), ("petla", loop), ("wstawanie", stop)):
        lows = []
        for k, f in enumerate(clip):
            gap = mm.feet_gap(SK, f)
            assert gap >= mm.FEET_MIN - 0.3, (name, k, "odstep stop", gap)
            W = SK.compose(*f)
            L, R = mm.leg_points(SK, W, "L"), mm.leg_points(SK, W, "R")
            thigh = min(mm.seg_dist(a[2], a[3], b[0], b[1]) for a in (L, R) for b in (L, R))
            assert thigh >= 13.0, (name, k, "stopa w udzie", thigh)
            P = body.skin(*f)
            pl, pr = P[np.isin(body.main, foot_ids["L"])], P[np.isin(body.main, foot_ids["R"])]
            sep = float(((pl @ dirs.T).min(axis=0) - (pr @ dirs.T).max(axis=0)).max())
            assert sep > 0.5, (name, k, "siatki stop sie przenikaja", sep)
            worst_gap, worst_sep, worst_thigh = min(worst_gap, gap), min(worst_sep, sep), min(worst_thigh, thigh)
            lows.append(float(P[:, 1].min()) - floor)
            if clip is loop:
                for side in "LR":
                    knee, foot = mm._leg_lows(body, SK, f, side, P)
                    assert -0.5 < knee - floor < 1.5 and -0.5 < foot - floor < 1.5, (k, side, knee - floor, foot - floor)
        lows = np.array(lows)
        if clip is loop:
            assert np.abs(lows).max() < 1.0, ("petla nad/pod podlozem", lows.min(), lows.max())
        else:
            assert lows.min() > -mm.FLOOR_PEN - 0.1 and lows.max() < 3.0, (name, lows.min(), lows.max())
        y = np.array([f[1][1] for f in clip])
        assert np.abs(np.diff(y)).max() < 6.0 and np.abs(np.diff(y, 2)).max() < 2.5, (name, "skok korzenia")
    print("   medytacja: odstep stop >= %.1f cm, siatki stop >= %.1f cm, stopa-udo >= %.1f cm, klek korzen %.1f cm" % (
        worst_gap, worst_sep, worst_thigh, loop[0][1][1]))



# ---------------------------------------------------------------- miecze na plecach (make_draw.py)

def _draw_compiled():
    import make_draw
    return make_draw.COMPILED


def _man_world(path, frame):
    """Macierze modelu klatki MAN z przesunieciami wezlow z probek (slot ZS_RIGHTHAND ma wlasne przesuniecie)."""
    m = manlib.Man(open(path, "rb").read())
    local = [l.copy() for l in SK.local]
    for k, ni in enumerate(m.nodes):
        (x, y, z, w), pos = m.decode(m.sample(frame, k))
        L = np.eye(4)
        L[:3, :3] = qmat(-x, -y, -z, w)
        L[:3, 3] = pos
        local[ni] = L
    W = []
    for i, L in enumerate(local):
        W.append(L if SK.parent[i] < 0 else W[SK.parent[i]] @ L)
    return W, m


def _msb_anis(path):
    """{nazwa: (definicja ANI, [(klatka, tag, argument)])} z MSB nakladki."""
    import build_overlay
    out, cur = {}, None
    for t_, _, body in build_overlay.chunks(open(path, "rb").read()):
        if t_ == 0xF520:
            a = build_overlay.parse_ani(body)
            cur = a["name"]
            out[cur] = (a, [])
        elif t_ == 0xF5A3 and cur:
            fr, = struct.unpack_from("<i", body, 0)
            lines = body[4:].decode("latin1").split("\n")
            out[cur][1].append((fr, lines[0], lines[1]))
        elif not (0xF5A0 <= t_ <= 0xF5AF):
            cur = None
    return out


def _back_pos(key):
    import make_draw
    for line in open(make_draw.INI, encoding="ascii"):
        if line.startswith(key + "="):
            v = [float(x) for x in line.split("=", 1)[1].split()]
            M = np.eye(4)
            M[:3, :] = np.array(v).reshape(3, 4)
            return M
    raise KeyError(key)


def _draw_ready():
    import make_draw
    ok = os.path.exists(make_draw.INI) and all(os.path.exists(os.path.join(_draw_compiled(), o + ".MSB"))
                                               for o in list(make_draw.OVERLAYS.values()) + [make_draw.BELT])
    if not ok:
        print("  (pominiety - brak nakladek make_draw.py w data_w3)")
    return ok


def _rot_deg(A, B):
    return math.degrees(math.acos(max(-1.0, min(1.0, (np.trace(A.T @ B) - 1) / 2))))


def test_w3_draw_overlay_names_exist_in_base_humans():
    # nakladka z nazwa spoza bazowego HUMANS jest w calosci odrzucana przez silnik (Steam, NB i Kroniki maja te same
    # definicje dobycia - [notatki lokalne])
    import make_draw, build_overlay
    if not _draw_ready():
        return
    base = set()
    for t_, _, body in build_overlay.chunks(bytes(VFS.find("HUMANS.MSB").data)):
        if t_ in (0xF520, 0xF530, 0xF540):
            base.add(body[:body.index(b"\n")].decode("latin1"))
    for ov in list(make_draw.OVERLAYS.values()) + [make_draw.BELT]:
        anis = _msb_anis(os.path.join(_draw_compiled(), ov + ".MSB"))
        assert len(anis) == 7, (ov, sorted(anis))
        for n in anis:
            assert n in base, (ov, n)
            assert os.path.exists(os.path.join(_draw_compiled(), "%s-%s.MAN" % (ov, n))), (ov, n)


def test_w3_draw_handover_puts_sword_exactly_on_back_slot():
    # w chwili chwytu (S_xH, 1. klatka T_xH_2_xHRUN, ostatnia T_xHRUN_2_xH) miecz w dloni lezy tam, gdzie plugin stawia
    # wezel slotu na plecach (PosA / PosB wzgledem BIP01 SPINE2) - przelozenie bez skoku
    import make_draw
    if not _draw_ready():
        return
    S2, RH = SK.idx("BIP01 SPINE2"), SK.idx("ZS_RIGHTHAND")
    worst = (0.0, 0.0)
    for (w, P), ov in make_draw.OVERLAYS.items():
        M = _back_pos("Pos" + P)
        n = make_draw.names_for(w)
        man = lambda a: os.path.join(_draw_compiled(), "%s-%s.MAN" % (ov, a))
        for a, fr in ((n["hold"], 0), (n["draw2"], 0), (n["draw1"], -1), (n["sheathe1"], -1), (n["sheathe2"], 0)):
            m = manlib.Man(open(man(a), "rb").read())
            W, _ = _man_world(man(a), fr if fr >= 0 else m.frames - 1)
            R = np.linalg.inv(W[S2]) @ W[RH]
            dp = np.linalg.norm(R[:3, 3] - M[:3, 3])
            ang = _rot_deg(R[:3, :3], M[:3, :3])
            worst = max(worst, (dp, ang))
            assert dp < 0.8 and ang < 1.5, (ov, a, dp, ang)
    print("   przelozenie: najwieksza odchylka %.2f cm / %.2f st." % worst)


def test_w3_draw_chain_joins_and_ends_in_standard_grip():
    # styki T_RUN_2_xH -> S_xH -> T_xH_2_xHRUN i T_xHRUN_2_xH -> S_xH -> T_xH_2_RUN bez skoku (cale cialo), konce
    # (garda po dobyciu, postawa po schowaniu) ze zwyklym chwytem Gothica w slocie dloni
    import make_draw, make_pair
    if not _draw_ready():
        return
    grip = make_pair.grip_local(VFS, SK)
    RH = SK.idx("ZS_RIGHTHAND")

    def pose(ov, a, fr):
        p = os.path.join(_draw_compiled(), "%s-%s.MAN" % (ov, a))
        m = manlib.Man(open(p, "rb").read())
        return _man_world(p, fr if fr >= 0 else m.frames - 1)[0]

    def gap(A, B):
        return max(np.linalg.norm(a[:3, 3] - b[:3, 3]) for a, b in zip(A, B))
    for (w, P), ov in make_draw.OVERLAYS.items():
        n = make_draw.names_for(w)
        hold = pose(ov, n["hold"], 0)
        for a, fr in ((n["draw1"], -1), (n["draw2"], 0), (n["sheathe1"], -1), (n["sheathe2"], 0)):
            g = gap(pose(ov, a, fr), hold)
            assert g < 1.0, (ov, a, g)
        for a in (n["draw2"], n["sheathe2"]):
            W = pose(ov, a, -1)
            S = np.linalg.inv(W[SK.parent[RH]]) @ W[RH]
            assert _rot_deg(S[:3, :3], grip) < 1.5 and np.linalg.norm(S[:3, 3] - SK.local[RH][:3, 3]) < 0.8, (ov, a)


def test_w3_draw_events_follow_engine_draw_sequence():
    # DEF_FIGHTMODE xH w 1. klatce T_xH_2_xHRUN (bron do reki w chwili chwytu, jak natywnie), "" w przedostatniej klatce
    # T_xHRUN_2_xH (bron do slotu w chwili puszczenia); w biegu w chwili chwytu / puszczenia; nastepne jak w HUMANS
    import make_draw
    if not _draw_ready():
        return
    for (w, P), ov in make_draw.OVERLAYS.items():
        n = make_draw.names_for(w)
        anis = _msb_anis(os.path.join(_draw_compiled(), ov + ".MSB"))
        d2, ev = anis[n["draw2"]]
        assert (1, "DEF_FIGHTMODE", w) in ev and d2["next"] == n["stance"], (ov, ev)
        s1, ev = anis[n["sheathe1"]]
        nf = s1["last"] - s1["first"] + 1
        assert (nf - 1, "DEF_FIGHTMODE", "") in ev and s1["next"] == n["hold"], (ov, ev, nf)
        assert anis[n["draw1"]][0]["next"] == n["hold"] and anis[n["sheathe2"]][0]["next"] == "S_RUN"
        md, ev = anis[n["move_draw"]]
        ms, evs = anis[n["move_sheathe"]]
        assert md["layer"] == 2 and ms["layer"] == 2
        assert any(t == "DEF_FIGHTMODE" and a == w for _, t, a in ev)
        assert any(t == "DEF_FIGHTMODE" and a == "" for _, t, a in evs)
        total = (anis[n["draw1"]][0]["last"] + anis[n["draw2"]][0]["last"] - 1) / 25.0
        assert 0.5 < total < 0.8, (ov, total)


def test_w3_draw_belt_copies_native_1h_draw():
    # BELT: natywne dobycie 1H z HUMANS (te same ASC, klatki i zdarzenia), T_1H_2_RUN = T_RUN_2_1H wstecz
    import make_draw
    if not _draw_ready():
        return
    anis = _msb_anis(os.path.join(_draw_compiled(), make_draw.BELT + ".MSB"))
    a, ev = anis["T_1H_2_1HRUN"]
    assert a["asc"] == "HUM_1HSDRAWT0_A01.ASC" and (a["first"], a["last"]) == (4, 15) and (4, "DEF_FIGHTMODE", "1H") in ev
    r = anis["T_1H_2_RUN"][0]
    assert r["dir"] == "R" and r["next"] == "S_RUN" and (r["first"], r["last"]) == (1, 2)


def test_w3_draw_back_positions_lie_behind_the_back():
    # pozycje A (stal) i B (srebro) za plecami (lokalne -Z SPINE2), rekojesci nad prawym barkiem, ostrza w dol i w lewo
    # jak natywny miecz dwureczny; B obok A (srebro Geralta), nie w tym samym miejscu
    import make_draw
    if not _draw_ready():
        return
    A, B = _back_pos("PosA"), _back_pos("PosB")
    nat = SK.local[SK.idx("ZS_LONGSWORD")]
    for M in (A, B):
        assert M[2, 3] < -2.0, M[:3, 3]
        assert M[1, 3] < -10.0, M[:3, 3]
        assert np.dot(-M[:3, 0], -nat[:3, 0]) > 0.9, M[:3, 0]
    assert 6.0 < np.linalg.norm(A[:3, 3] - B[:3, 3]) < 25.0


# ---------------------------------------------------------------- szybki strzal z kuszy (make_xbow.py)

_XBOW = {}


def _xbow():
    """make_xbow.build (raz na przebieg testow); None bez zrzutow W3."""
    import make_xbow
    if "res" not in _XBOW:
        need = [os.path.join(make_xbow.WORK, c + ".json") for c in make_xbow.CLIPS.values()]
        seed = [os.path.join(make_xbow.SEED_DUMPS, c + ".json") for c in make_xbow.CLIPS.values()]
        if not all(os.path.exists(a) or os.path.exists(b) for a, b in zip(need, seed)):
            _XBOW["res"] = None
        else:
            _XBOW["res"] = make_xbow.build(VFS, SK, verbose=False)
    if _XBOW["res"] is None:
        print("  (pominiety - brak zrzutow W3 kuszy)")
    return _XBOW["res"]


def _humans_defs_of(game, files):
    """Definicje ANI bazowego HUMANS.MSB gry (zamontowane VDF jak w grze, nowsze wygrywaja); None bez gry."""
    import build_overlay
    paths = [os.path.join(game, "Data", f) for f in files]
    if not all(os.path.exists(p) for p in paths):
        return None
    vfs = zenkit.Vfs()
    for p in paths:
        vfs.mount_disk(p, clobber=zenkit.VfsOverwriteBehavior.NEWER)
    return build_overlay.humans_ani_defs(vfs)


def test_xbow_carriers_exist_in_humans_of_three_games():
    # zero nowych nazw: nosniki kuszy to nazwy gestow Znakow - istnieja w bazowym HUMANS Steam G2, New Balance i Kronik
    import make_xbow, build_overlay
    names = list(make_xbow.XBOW_ANIS.values())
    assert len(set(names)) == len(names) == 6
    games = {"steam": build_overlay.humans_ani_defs(VFS),
             "nb": _humans_defs_of(os.environ.get("GOTHIC2_NB_DIR", ""), ("Anims.vdf", "Anims_Addon.vdf", "AB_Anims.vdf")),
             "km": _humans_defs_of(os.environ.get("GOTHIC_KM_DIR", ""),
                                   ("Anims.vdf", "Anims_Addon.vdf", "KM_Anims.mod"))}
    for g, defs in games.items():
        if defs is None:
            print("  (%s - brak gry, pominiete)" % g)
            continue
        assert all(n in defs for n in names), (g, [n for n in names if n not in defs])


def test_xbow_tables_match_zdismember():
    # zdcore::XbowClipAni (Core\Crossbow.cpp) = XBOW_ANIS z make_xbow.py; nosniki = gesty Znakow (make_signs)
    import re, make_xbow, make_signs
    src = open(os.path.join(HERE, "..", "..", "zDismember", "Core", "Crossbow.cpp"), encoding="latin1").read()
    for standing, arr in ((True, "STAND"), (False, "MOVE")):
        m = re.search(r"%s\[XCLIP_COUNT\]\s*=\s*\{(.*?)\};" % arr, src, re.S)
        assert m, arr
        got = re.findall(r'"(\w+)"', m.group(1))
        assert got == [make_xbow.XBOW_ANIS[(k, standing)] for k in make_xbow.KINDS], (arr, got)
    signs = set(make_signs.SIGN_ANIS.values()) | set(make_signs.SIGN_ANIS_STAND.values())
    assert set(make_xbow.XBOW_ANIS.values()) <= signs
    assert make_xbow.LAYER == make_signs.LAYER


def test_xbow_overlay_files_layer_and_no_def_tags():
    # MSB / MAN z make_xbow.py: 6 klipow na warstwie LAYER, bez nastepnej animacji (petle prowadzi plugin), zadnych zdarzen
    # DEF_* (tryb walki, sloty, amunicja) - tylko dzwieki; ZS_LEFTHAND i ZS_CROSSBOW kluczowane
    import make_xbow
    msb = os.path.join(make_xbow.COMPILED, make_xbow.OVERLAY + ".MSB")
    if not os.path.exists(msb):
        print("  (pominiety - brak nakladki, uruchom make_xbow.py)")
        return
    data = open(msb, "rb").read()
    k = data.find(struct.pack("<H", 0xF520))   # naglowek (DEF_HIT_LIMB modelu) jak w kazdej naszej nakladce
    assert k > 0 and b"DEF_" not in data[k:]
    anis = read_msb_anis(msb)
    assert sorted(anis) == sorted(make_xbow.XBOW_ANIS.values()), sorted(anis)
    for name, a in anis.items():
        m = manlib.Man(open(os.path.join(make_xbow.COMPILED, "%s-%s.MAN" % (make_xbow.OVERLAY, name)), "rb").read())
        assert a.layer == make_xbow.LAYER and m.layer == make_xbow.LAYER and a.next == "", (name, a.layer, a.next)
        assert m.frames == a.last_frame - a.first_frame + 1, name
        keyed = {SK.names[i] for i in m.nodes}
        assert {"ZS_LEFTHAND", "ZS_CROSSBOW", "ZS_RIGHTHAND", "BIP01 L HAND"} <= keyed, name


def test_xbow_stand_variant_in_place_with_planted_feet():
    r = _xbow()
    if r is None:
        return
    feet = (SK.idx("BIP01 L FOOT"), SK.idx("BIP01 R FOOT"))
    for kind in ("IN", "LOOP", "SHOOT"):
        st, mv = r["clips"][(kind, True)], r["clips"][(kind, False)]
        assert len(st["frames"]) == len(mv["frames"]), kind
        assert all(abs(root[0]) < 1e-6 and abs(root[2]) < 1e-6 for _, root in st["frames"]), kind
        W0 = SK.compose(*st["frames"][0])
        slide = max(np.linalg.norm(SK.compose(*f)[i][:3, 3] - W0[i][:3, 3]) for f in st["frames"] for i in feet)
        assert slide <= 1.0, (kind, slide)
        names = {SK.names[i] for i in mv["nodes"]}
        assert "BIP01 L THIGH" not in names and "BIP01 SPINE" in names, kind


def test_xbow_hand_reaches_crossbow_on_back():
    # dlon chwyta kusze na plecach za przod (GRIP_CM - srodek plecow, wysokosc ledzwi): w klatce chwytu (IN) i odlozenia
    # (SHOOT) kusza w dloni (slot ZS_LEFTHAND z przesunieciem) lezy dokladnie tam, gdzie wisi na plecach: <= 1 cm, <= 1 st.;
    # po IK dlon jest na chwycie (dosuniecie w dloni najwyzej 2 cm)
    import make_xbow
    r = _xbow()
    if r is None:
        return
    for kind, k in (("IN", r["diag"]["k_g"]), ("SHOOT", r["diag"]["k_r"])):
        for standing in (True, False):
            c = r["clips"][(kind, standing)]
            fr = c["frames"]
            if not standing:   # wariant ruchu: tulow nad miednica biegu - liczymy na pozie z kregoslupem z klatki
                ref = make_signs_run()
                local = [R.copy() for R in ref[0]]
                for i in c["nodes"]:
                    local[i] = fr[k][0][i]
                fr = [(local, ref[1])] * (k + 1)
            cm, deg = make_xbow.transfer_error(SK, fr, k, r["cbow_local"], c["slots"])
            assert cm <= 1.0 and deg <= 1.0, (kind, standing, cm, deg)
        assert r["diag"][kind]["ik_rest"] <= 2.0, (kind, r["diag"][kind]["ik_rest"])


def test_xbow_arm_stays_natural():
    # zgloszenie 23.10 (reka wygieta za plecy do wezla nad prawym barkiem): IK tylko domyka tor W3 - korekta dloni <= REACH_CAP,
    # lokiec najwyzej 20 cm od toru W3, kat w lokciu >= 40 st., dlon nie przy glowie (>= 30 cm), chwyt nizej niz bark
    import make_xbow
    r = _xbow()
    if r is None:
        return
    for kind in ("IN", "SHOOT"):
        c, raw = r["clips"][(kind, True)], r["diag"][kind]["raw"]
        assert r["diag"][kind]["ik_fix"] <= make_xbow.REACH_CAP + 1e-6, kind
        for k, (f, f0) in enumerate(zip(c["frames"], raw)):
            W, W0 = SK.compose(*f), SK.compose(*f0)
            el = np.linalg.norm(W[SK.idx("BIP01 L FOREARM")][:3, 3] - W0[SK.idx("BIP01 L FOREARM")][:3, 3])
            assert el <= 20.0, (kind, k, el)
            assert make_xbow.elbow_angle(SK, *f) >= 40.0, (kind, k)
            assert np.linalg.norm(W[SK.idx("BIP01 L HAND")][:3, 3] - W[SK.idx("BIP01 HEAD")][:3, 3]) >= 30.0, (kind, k)
    W = SK.compose(*r["clips"][("IN", True)]["frames"][r["diag"]["k_g"]])
    assert make_xbow.hand_grip(SK, W)[1] < W[SK.idx("BIP01 L UPPERARM")][:3, 3][1] - 10.0


def make_signs_run():
    import make_signs
    if "run" not in _XBOW:
        _XBOW["run"] = make_signs.mean_pose(VFS, SK, make_signs.RUN_REF)
    return _XBOW["run"]


def test_xbow_aim_loop_points_forward_and_level():
    # petla celowania: lufa kuszy w dloni (-X ZS_LEFTHAND) na wprost postaci, poziomo (<= 3 st.)
    import make_xbow
    r = _xbow()
    if r is None:
        return
    for local, root in r["clips"][("LOOP", True)]["frames"]:
        d = make_xbow.muzzle_dir(SK, local, root)
        ang = math.degrees(math.acos(max(-1.0, min(1.0, d[2] / np.linalg.norm(d)))))
        assert ang <= 3.0, ang


def test_xbow_shot_recoils_and_settles():
    # strzal: odrzut z addytywu (lufa w gore o 20+ st. w pierwszych klatkach), na koncu odrzutu poza celowania jak w petli,
    # potem odlozenie kuszy na plecy (koniec klipu: lufa daleko od celowania)
    import make_xbow
    r = _xbow()
    if r is None:
        return
    fr = r["clips"][("SHOOT", True)]["frames"]
    n_add = r["diag"]["n_add"]
    pitch = [math.degrees(math.asin(max(-1.0, min(1.0, make_xbow.muzzle_dir(SK, *f)[1])))) for f in fr]
    assert max(pitch[:n_add]) >= 20.0, pitch[:n_add]
    assert abs(pitch[0]) <= 5.0 and abs(pitch[n_add]) <= 5.0, (pitch[0], pitch[n_add])
    d = make_xbow.muzzle_dir(SK, *fr[-1])
    assert d[2] < 0.7, d


def test_xbow_ini_matches_clips():
    # crossbow_w3.ini (make_xbow.py) - dlugosci jak MAN, chwyt w wejsciu, belt przed odlozeniem, odlozenie przed koncem
    import make_xbow
    r = _xbow()
    if r is None or not os.path.exists(make_xbow.INI):
        print("  (pominiety - brak crossbow_w3.ini)")
        return
    vals = {}
    for line in open(make_xbow.INI):
        if "=" in line and not line.startswith(";"):
            k, v = line.strip().split("=", 1)
            vals[k] = int(v)
    for kind, key in (("IN", "InMs"), ("LOOP", "LoopMs"), ("SHOOT", "ShootMs")):
        assert vals[key] == round((len(r["clips"][(kind, True)]["frames"]) - 1) / make_xbow.FPS * 1000), key
    assert 0 < vals["GrabMs"] < vals["InMs"]
    assert vals["FireMs"] < vals["ReleaseMs"] < vals["ShootMs"]
    assert vals == r["timing"], (vals, r["timing"])


if __name__ == "__main__":
    failed = 0
    tests = [(n, f) for n, f in sorted(globals().items()) if n.startswith("test_")]
    for name, fn in tests:
        try:
            fn()
            print("ok  ", name)
        except Exception:
            failed += 1
            print("FAIL", name)
            traceback.print_exc(limit=2)
    print(f"\n{len(tests) - failed}/{len(tests)} OK")
    sys.exit(1 if failed else 0)
