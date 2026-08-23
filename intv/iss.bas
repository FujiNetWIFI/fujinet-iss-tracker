    ' ------------------------------------------------------------------------
    ' ISS Tracker for Intellivision + FujiNet
    '
    ' Displays a world map with a color-cycling satellite MOB at the current
    ' position of the International Space Station, with latitude, longitude
    ' and timestamp text below, refreshed every 60 seconds from
    ' http://api.open-notify.org/iss-now.json via the FujiNet network device
    ' (JSON channel mode -- the JSON is parsed FujiNet-side, exactly like the
    ' other clients in this repository).
    '
    ' Map data (map.bas) is generated from iss-intv.png by
    ' workfiles/png2map.py -- see workfiles/README.md.
    ' ------------------------------------------------------------------------

    INCLUDE "constants.bas"

    ' Satellite hotspot: map pixel -> MOB coordinate. The STIC's visible
    ' background origin is at MOB (8,8), and the sprite center is (4,4)
    ' inside its 8x8 card, so net +4 on both axes puts the sprite's center
    ' on the computed map pixel. Verified against known landmarks in jzIntv.
    CONST SAT_XOFS = 4
    CONST SAT_YOFS = 4

    ' Text rows (BACKTAB offset = row*20). Cell 180 (row 9, column 0)
    ' carries the CS_ADVANCE bit that turns the text area black -- nothing
    ' may ever write it, so row-9 text starts one cell in.
    CONST POS_LAT = 181
    CONST POS_LON = 201
    CONST POS_TS  = 220

    ' App scratch RAM. fujinet.bas owns $9100-$917F; ours lives above it.
    CONST SC_LAT  = $9200   ' latitude ASCII, NUL-terminated ("-51.1234")
    CONST SC_LON  = $9210   ' longitude ASCII ("-179.9999")
    CONST SC_TS   = $9220   ' epoch ASCII digits ("1653813276")
    CONST SC_WORK = $9230   ' mutable digit workspace for long division
    CONST SC_LINE = $9240   ' composed "YYYY-MM-DD HH:MM:SS" + NUL

    ' Shared across procedures (DIM must precede first use in file order):
    ' #tmp catches every PEEK result -- IntyBASIC v1.4.2 silently drops the
    ' AND 255 mask when the destination is 8-bit (see fujinet.bas), so PEEKs
    ' always land in a 16-bit variable. fetch_ok is the whole-cycle verdict.
    DIM fetch_ok
    DIM #tmp

    GOTO main

    INCLUDE "fujinet.bas"
    INCLUDE "map.bas"

    ' ------------------------------------------------------------------------
    ' ROM literals. Numeric DATA on purpose: string DATA is converted to GROM
    ' character codes (ASCII-32), which would corrupt bytes headed for the
    ' wire. Only PRINT string literals may use quoted text.
    ' ------------------------------------------------------------------------

    ' "N:HTTP://api.open-notify.org/iss-now.json" (41 bytes)
lit_url:
    DATA 78,58,72,84,84,80,58,47,47,97,112,105,46,111
    DATA 112,101,110,45,110,111,116,105,102,121,46,111,114,103
    DATA 47,105,115,115,45,110,111,119,46,106,115,111,110
    CONST LEN_URL = 41

    ' "/iss_position/latitude" (22 bytes)
lit_q_lat:
    DATA 47,105,115,115,95,112,111,115,105,116,105,111,110,47
    DATA 108,97,116,105,116,117,100,101
    CONST LEN_Q_LAT = 22

    ' "/iss_position/longitude" (23 bytes)
lit_q_lon:
    DATA 47,105,115,115,95,112,111,115,105,116,105,111,110,47
    DATA 108,111,110,103,105,116,117,100,101
    CONST LEN_Q_LON = 23

    ' "/timestamp" (10 bytes)
lit_q_ts:
    DATA 47,116,105,109,101,115,116,97,109,112
    CONST LEN_Q_TS = 10

    ' MOB color cycle: white, yellow, red, orange, pink, purple, cyan, tan.
    ' Deliberately no green/blue/black so the satellite never vanishes into
    ' the map. Pastels carry the $1000 upper color bit -- read via a 16-bit
    ' variable and ADD into the SPRITE f operand.
spr_colors:
    DATA $0007,$0006,$0002,$1002,$1004,$1007,$1001,$0003

month_days:
    DATA 31,28,31,30,31,30,31,31,30,31,30,31

    ' ------------------------------------------------------------------------
    ' draw_str: draw the NUL-terminated ASCII string at #ds_src to BACKTAB
    ' offset ds_pos in foreground color ds_col (on the current color stack),
    ' padding with blanks to a field of ds_pad cells (erases stale longer
    ' values).
    ' ------------------------------------------------------------------------
    DIM ds_pos, ds_pad, ds_i, ds_col
    DIM #ds_src
draw_str: PROCEDURE
    ds_i = 0
ds_char:
    IF ds_i >= ds_pad THEN RETURN
    #tmp = PEEK(#ds_src + ds_i) AND 255
    IF (#tmp < 32) OR (#tmp > 126) THEN GOTO ds_blank
    #BACKTAB(ds_pos + ds_i) = (#tmp - 32) * 8 + ds_col
    ds_i = ds_i + 1
    GOTO ds_char
ds_blank:
    IF ds_i >= ds_pad THEN RETURN
    #BACKTAB(ds_pos + ds_i) = ds_col
    ds_i = ds_i + 1
    GOTO ds_blank
END

    ' ------------------------------------------------------------------------
    ' draw_coord: draw a coordinate at #ds_src to BACKTAB offset ds_pos as
    ' unsigned degrees in white followed by a tan degree mark (GROM has no
    ' degree symbol -- '*' stands in) and tan hemisphere letter, e.g.
    ' "51.3363*N". The sign picks hem_neg over hem_pos and is not drawn.
    ' The value is right-justified in a 9-cell field so the degree mark and
    ' hemisphere land at fixed columns (ds_pos+9/+10) and line up across
    ' rows; a trailing blank makes it a 12-cell field.
    ' ------------------------------------------------------------------------
    DIM hem_pos, hem_neg, hem_ch, dc_len
draw_coord: PROCEDURE
    #tmp = PEEK(#ds_src) AND 255
    IF #tmp = 45 THEN hem_ch = hem_neg : #ds_src = #ds_src + 1 ELSE hem_ch = hem_pos
    dc_len = 0
dc_meas:
    #tmp = PEEK(#ds_src + dc_len) AND 255
    IF (#tmp >= 32) AND (#tmp <= 126) AND (dc_len < 9) THEN dc_len = dc_len + 1 : GOTO dc_meas
    ds_i = 0
dc_lead:
    IF ds_i < 9 - dc_len THEN #BACKTAB(ds_pos + ds_i) = 7 : ds_i = ds_i + 1 : GOTO dc_lead
dc_char:
    IF ds_i >= 9 THEN GOTO dc_suffix
    #tmp = PEEK(#ds_src + ds_i - (9 - dc_len)) AND 255
    #BACKTAB(ds_pos + ds_i) = (#tmp - 32) * 8 + 7
    ds_i = ds_i + 1
    GOTO dc_char
dc_suffix:
    #BACKTAB(ds_pos + 9) = (42 - 32) * 8 + 3
    #BACKTAB(ds_pos + 10) = (hem_ch - 32) * 8 + 3
    #BACKTAB(ds_pos + 11) = 7
END

    ' ------------------------------------------------------------------------
    ' fetch_query: run one JSON query against the open channel. In: #q_src
    ' (ROM pointer to the query path), q_len, #q_dst (destination scratch
    ' buffer, 16 bytes). The value's ASCII lands NUL-terminated at #q_dst.
    ' Clears fetch_ok on any failure.
    ' ------------------------------------------------------------------------
    DIM q_len, cp_i
    DIM #q_src, #q_dst
fetch_query: PROCEDURE
    #fn_txlen = 0
    #fn_src = #q_src : fn_len = q_len : GOSUB fn_putstr
    ' Explicit NUL terminator: the firmware's rs232_set_json_query() copies
    ' the payload into an uninitialized stack buffer and reads it as a C
    ' string, so without this the query inherits tail garbage from the
    ' previous (longer) query and silently matches nothing.
    POKE (FN_TX + #fn_txlen), 0
    #fn_txlen = #fn_txlen + 1
    GOSUB net_query
    IF fn_ok = 0 THEN fetch_ok = 0 : RETURN
    GOSUB net_status
    IF fn_ok = 0 THEN fetch_ok = 0 : RETURN
    IF #net_avail = 0 THEN fetch_ok = 0 : RETURN
    #net_readlen = #net_avail
    IF #net_readlen > 15 THEN #net_readlen = 15
    GOSUB net_read
    IF fn_ok = 0 THEN fetch_ok = 0 : RETURN
    IF #net_gotlen > 15 THEN #net_gotlen = 15
    FOR cp_i = 0 TO 15
        POKE (#q_dst + cp_i), 0
    NEXT cp_i
    cp_i = 0
cq_copy:
    IF cp_i >= #net_gotlen THEN RETURN
    #tmp = PEEK(FN_RX + cp_i) AND 255
    IF #tmp < 32 THEN RETURN
    POKE (#q_dst + cp_i), #tmp
    cp_i = cp_i + 1
    GOTO cq_copy
END

    ' ------------------------------------------------------------------------
    ' fetch_iss: one full cycle. Open the URL, switch the channel to JSON,
    ' let the deferred HTTP GET settle, parse, pull the three values into
    ' SC_LAT/SC_LON/SC_TS, close. fetch_ok = 1 on full success.
    ' ------------------------------------------------------------------------
fetch_iss: PROCEDURE
    fetch_ok = 1
    #fn_txlen = 0
    #fn_src = VARPTR lit_url(0) : fn_len = LEN_URL : GOSUB fn_putstr
    GOSUB net_open
    IF fn_ok = 0 THEN fetch_ok = 0 : RETURN
    ' The GET is deferred until the first STATUS and can report a partial
    ' byte count while the body is still arriving: poll until two
    ' consecutive equal nonzero readings (api_call's proven pattern).
    ' This must happen BEFORE the switch to JSON mode: in JSON mode STATUS
    ' reports the query-result length (zero until a query is set, with an
    ' END_OF_FILE error byte that net_status treats as failure), not the
    ' protocol byte count -- rs232_status_channel() in fujinet-firmware.
    #ac_prev = 0
    FOR ac_i = 0 TO 19
        WAIT
        GOSUB net_status
        IF fn_ok = 0 THEN GOTO fi_fail
        IF (#net_avail > 0) AND (#net_avail = #ac_prev) THEN EXIT FOR
        #ac_prev = #net_avail
    NEXT ac_i
    cm_mode = CHANMODE_JSON : GOSUB net_chanmode
    IF fn_ok = 0 THEN GOTO fi_fail
    GOSUB net_parse
    IF fn_ok = 0 THEN GOTO fi_fail
    #q_src = VARPTR lit_q_lat(0) : q_len = LEN_Q_LAT : #q_dst = SC_LAT : GOSUB fetch_query
    #q_src = VARPTR lit_q_lon(0) : q_len = LEN_Q_LON : #q_dst = SC_LON : GOSUB fetch_query
    #q_src = VARPTR lit_q_ts(0)  : q_len = LEN_Q_TS  : #q_dst = SC_TS  : GOSUB fetch_query
    GOSUB net_close
    RETURN
fi_fail:
    fetch_ok = 0
    GOSUB net_close
END

    ' ------------------------------------------------------------------------
    ' parse_coord: parse "[-]DDD[.ffff]" at #ds_src into p_neg + #p_mag
    ' (integer degrees; the fraction is below map pixel resolution).
    ' ------------------------------------------------------------------------
    DIM p_neg
    DIM #p_mag
parse_coord: PROCEDURE
    p_neg = 0
    cp_i = 0
    #tmp = PEEK(#ds_src) AND 255
    IF #tmp = 45 THEN p_neg = 1 : cp_i = 1
    #p_mag = 0
pc_digit:
    #tmp = PEEK(#ds_src + cp_i) AND 255
    IF (#tmp < 48) OR (#tmp > 57) THEN RETURN
    #p_mag = #p_mag * 10 + #tmp - 48
    IF #p_mag > 999 THEN #p_mag = 999 : RETURN
    cp_i = cp_i + 1
    GOTO pc_digit
END

    ' ------------------------------------------------------------------------
    ' update_position: SC_LAT/SC_LON -> sat_mx/sat_my MOB coordinates via the
    ' lon2x/lat2y tables in map.bas.
    ' ------------------------------------------------------------------------
    DIM have_fix, sat_mx, sat_my
    DIM #idx
update_position: PROCEDURE
    #ds_src = SC_LON : GOSUB parse_coord
    IF #p_mag > 180 THEN #p_mag = 180
    IF p_neg THEN #idx = 180 - #p_mag ELSE #idx = 180 + #p_mag
    #tmp = PEEK(VARPTR lon2x(0) + #idx)
    sat_mx = #tmp + SAT_XOFS
    #ds_src = SC_LAT : GOSUB parse_coord
    IF #p_mag > 90 THEN #p_mag = 90
    IF p_neg THEN #idx = 90 - #p_mag ELSE #idx = 90 + #p_mag
    #tmp = PEEK(VARPTR lat2y(0) + #idx)
    sat_my = #tmp + SAT_YOFS
    have_fix = 1
END

    ' ------------------------------------------------------------------------
    ' update_sprite: step the color cycle and (re)position MOB 0.
    ' ------------------------------------------------------------------------
    DIM spr_ci
update_sprite: PROCEDURE
    IF have_fix = 0 THEN SPRITE 0, 0, 0, 0 : RETURN
    spr_ci = (spr_ci + 1) AND 7
    #tmp = PEEK(VARPTR spr_colors(0) + spr_ci)
    SPRITE 0, sat_mx + VISIBLE, sat_my, SPR63 + #tmp
END

    ' ------------------------------------------------------------------------
    ' div_digits: long-divide the dw_len-digit ASCII number at SC_WORK by
    ' dv_d in place (quotient keeps leading zeros); remainder in dv_rem.
    ' #cur maxes at 59*10+9 = 599, so 16-bit is required and sufficient.
    ' ------------------------------------------------------------------------
    DIM dv_d, dv_rem, dd_i, dw_len
    DIM #cur
div_digits: PROCEDURE
    dv_rem = 0
    FOR dd_i = 0 TO dw_len - 1
        #cur = dv_rem * 10 + (PEEK(SC_WORK + dd_i) AND 255) - 48
        POKE (SC_WORK + dd_i), (#cur / dv_d) + 48
        dv_rem = #cur % dv_d
    NEXT dd_i
END

    ' ------------------------------------------------------------------------
    ' fmt2: two zero-padded decimal digits of fm_val at SC_LINE + fm_pos.
    ' ------------------------------------------------------------------------
    DIM fm_pos, fm_val
fmt2: PROCEDURE
    POKE (SC_LINE + fm_pos), (fm_val / 10) + 48
    POKE (SC_LINE + fm_pos + 1), (fm_val % 10) + 48
END

    ' ------------------------------------------------------------------------
    ' ts_to_datetime: epoch ASCII digits at SC_TS -> "YYYY-MM-DD HH:MM:SS"
    ' at SC_LINE. The epoch (~1.7e9) exceeds 16 bits, so the seconds/minutes/
    ' hours are peeled off with decimal-string long division; only the day
    ' count (~20,700) ever becomes a binary number. All values non-negative,
    ' so IntyBASIC's unsigned-only division is safe throughout.
    ' ------------------------------------------------------------------------
    DIM ts_ok, dt_sec, dt_min, dt_hour, dt_day, dt_mon, leap_f
    DIM #days, #year
ts_to_datetime: PROCEDURE
    ts_ok = 0
    dw_len = 0
td_copy:
    IF dw_len >= 12 THEN GOTO td_have
    #tmp = PEEK(SC_TS + dw_len) AND 255
    IF (#tmp < 48) OR (#tmp > 57) THEN GOTO td_have
    POKE (SC_WORK + dw_len), #tmp
    dw_len = dw_len + 1
    GOTO td_copy
td_have:
    IF dw_len = 0 THEN RETURN
    dv_d = 60 : GOSUB div_digits : dt_sec = dv_rem
    dv_d = 60 : GOSUB div_digits : dt_min = dv_rem
    dv_d = 24 : GOSUB div_digits : dt_hour = dv_rem
    #days = 0
    FOR dd_i = 0 TO dw_len - 1
        #days = #days * 10 + (PEEK(SC_WORK + dd_i) AND 255) - 48
    NEXT dd_i
    ' civil date from days since 1970-01-01 (good through 2149)
    #year = 1970
td_year:
    leap_f = 0
    IF (#year AND 3) = 0 THEN leap_f = 1
    IF (#year % 100) = 0 THEN leap_f = 0
    IF (#year % 400) = 0 THEN leap_f = 1
    #cur = 365 + leap_f
    IF #days >= #cur THEN #days = #days - #cur : #year = #year + 1 : GOTO td_year
    dt_mon = 0
td_month:
    #tmp = PEEK(VARPTR month_days(0) + dt_mon)
    IF dt_mon = 1 THEN #tmp = #tmp + leap_f
    IF #days >= #tmp THEN #days = #days - #tmp : dt_mon = dt_mon + 1 : GOTO td_month
    dt_day = #days + 1
    ' compose "YYYY-MM-DD HH:MM:SS"
    POKE (SC_LINE + 0), (#year / 1000) + 48
    POKE (SC_LINE + 1), ((#year / 100) % 10) + 48
    POKE (SC_LINE + 2), ((#year / 10) % 10) + 48
    POKE (SC_LINE + 3), (#year % 10) + 48
    POKE (SC_LINE + 4), 45
    fm_pos = 5 : fm_val = dt_mon + 1 : GOSUB fmt2
    POKE (SC_LINE + 7), 45
    fm_pos = 8 : fm_val = dt_day : GOSUB fmt2
    POKE (SC_LINE + 10), 32
    fm_pos = 11 : fm_val = dt_hour : GOSUB fmt2
    POKE (SC_LINE + 13), 58
    fm_pos = 14 : fm_val = dt_min : GOSUB fmt2
    POKE (SC_LINE + 16), 58
    fm_pos = 17 : fm_val = dt_sec : GOSUB fmt2
    POKE (SC_LINE + 19), 0
    ts_ok = 1
END

    ' ------------------------------------------------------------------------
    ' draw_position: refresh the three text rows from the fetched strings.
    ' ------------------------------------------------------------------------
draw_position: PROCEDURE
    ' red labels, white values, tan degree mark + hemisphere, yellow clock
    PRINT AT POS_LAT COLOR 2, "LAT "
    #ds_src = SC_LAT : ds_pos = POS_LAT + 4 : hem_pos = 78 : hem_neg = 83 : GOSUB draw_coord
    PRINT AT POS_LON COLOR 2, "LON "
    #ds_src = SC_LON : ds_pos = POS_LON + 4 : hem_pos = 69 : hem_neg = 87 : GOSUB draw_coord
    GOSUB ts_to_datetime
    IF ts_ok THEN #ds_src = SC_LINE : ds_pos = POS_TS : ds_pad = 20 : ds_col = 6 : GOSUB draw_str
END

    ' ------------------------------------------------------------------------
    ' main
    ' ------------------------------------------------------------------------
    DIM #t, #dtot
main:
    MODE 0, STACK_BLUE, STACK_BLACK, STACK_BLACK, STACK_BLACK
    WAIT
    DEFINE DEF00, 16, screen_bitmaps_0
    WAIT
    DEFINE DEF16, 16, screen_bitmaps_1
    WAIT
    DEFINE DEF32, 16, screen_bitmaps_2
    WAIT
    DEFINE DEF48, 15, screen_bitmaps_3
    WAIT
    DEFINE DEF63, 1, satellite_bmp
    WAIT
    SCREEN screen_cards
    SPRITE 0, 0, 0, 0
    have_fix = 0
    spr_ci = 0

    PRINT AT POS_LAT COLOR 7, "CONNECTING..."
    GOSUB fn_wait_mailbox
    IF fn_ok = 0 THEN GOTO no_mailbox

main_loop:
    GOSUB fetch_iss
    IF fetch_ok THEN GOSUB update_position : GOSUB draw_position
    ' 60 seconds between fetches; keep the satellite cycling meanwhile.
    ' After a failed cycle retry in ~5s instead -- the very first STATUS
    ' can outrun the bridge's transaction deadline while the cold HTTP GET
    ' completes, and a transient failure shouldn't cost a whole minute.
    ' Counted WAITs, not FRAME comparisons -- FRAME wraps at 65535.
    IF NTSC THEN #dtot = 3600 ELSE #dtot = 3000
    IF fetch_ok = 0 THEN #dtot = #dtot / 12
    FOR #t = 1 TO #dtot
        WAIT
        IF (#t AND 7) = 0 THEN GOSUB update_sprite
    NEXT #t
    GOTO main_loop

no_mailbox:
    PRINT AT POS_LAT COLOR 7, "NO FUJINET MAILBOX"
nm_halt:
    WAIT
    GOTO nm_halt
