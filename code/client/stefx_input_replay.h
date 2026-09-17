// Diagnostic input only. The temporary probe ISO supplies this opt-in file;
// release packages must not contain it. No host input or script state is used.
static void STEFX_ApplyInputReplay(usercmd_t *cmd, const vec3_t initialAngles)
{
	struct Row { int start, end, forward, right, up, pitch, yaw, buttons; };
	static Row rows[32];
	static int count = 0, checked = 0, base = -1, previous = -1, lastRow = -2;
	static char map[64];
	extern bool in_camera;
	if (!cmd || cls.state != CA_ACTIVE) return;
	if (!checked)
	{
		checked = 1;
		FILE *file = fopen("D:\\ef_sp_input_replay.txt", "r");
		if (!file) return;
		char line[256];
		bool valid = fgets(line, sizeof(line), file) &&
			sscanf(line, "STEFX_INPUT_REPLAY_V1 %63s", map) == 1;
		int lastEnd = 0;
		while (valid && fgets(line, sizeof(line), file))
		{
			Row row;
			char extra;
			if (count == 32 || sscanf(line, "%d %d %d %d %d %d %d %d %c",
				&row.start, &row.end, &row.forward, &row.right, &row.up,
				&row.pitch, &row.yaw, &row.buttons, &extra) != 8 ||
				row.start < lastEnd || row.end <= row.start || row.end > 600000 ||
				row.forward < -127 || row.forward > 127 || row.right < -127 || row.right > 127 ||
				row.up < -127 || row.up > 127 || row.pitch < -360 || row.pitch > 360 ||
				row.yaw < -360 || row.yaw > 360 || row.buttons < 0 ||
				(row.buttons & ~(BUTTON_ATTACK | BUTTON_ALT_ATTACK | BUTTON_USE)))
			{ valid = false; break; }
			rows[count++] = row;
			lastEnd = row.end;
		}
		fclose(file);
		if (!valid || !count) { count = 0; XBL("STEFX_INPUT_REPLAY: rejected invalid file"); return; }
		XBLF("STEFX_INPUT_REPLAY: loaded map=%s rows=%d", map, count);
	}
	if (!count) return;
	if (Q_stricmp(map, Cvar_VariableString("mapname")))
	{
		count = 0;
		XBL("STEFX_INPUT_REPLAY: stopped on map mismatch");
		return;
	}
	const int now = cl.serverTime;
	if (base < 0) base = now;
	if (now < base || (previous >= 0 && now < previous))
	{ count = 0; XBL("STEFX_INPUT_REPLAY: stopped on time reset"); return; }
	int delta = previous >= 0 ? now - previous : 0;
	previous = now;
	if (delta > 100) delta = 100;
	const int elapsed = now - base;
	if (elapsed >= rows[count - 1].end)
	{ count = 0; XBL("STEFX_INPUT_REPLAY: complete"); return; }
	if (in_camera) return;
	int selected = -1;
	for (int i = 0; i < count; ++i)
		if (elapsed >= rows[i].start && elapsed < rows[i].end) { selected = i; break; }
	// Neutral gaps make this process-local run independent of retained pad input.
	cmd->forwardmove = cmd->rightmove = cmd->upmove = 0;
	cmd->buttons = 0;
	VectorCopy(initialAngles, cl.viewangles);
	if (selected >= 0)
	{
		const Row &row = rows[selected];
		cmd->forwardmove = row.forward; cmd->rightmove = row.right; cmd->upmove = row.up;
		cmd->buttons = row.buttons;
		cl.viewangles[PITCH] += row.pitch * (delta * 0.001f);
		cl.viewangles[YAW] += row.yaw * (delta * 0.001f);
	}
	if (selected != lastRow)
	{
		XBLF("STEFX_INPUT_REPLAY: row=%d elapsed=%d origin=(%g,%g,%g) health=%d",
			selected, elapsed, cl.frame.ps.origin[0], cl.frame.ps.origin[1],
			cl.frame.ps.origin[2], cl.frame.ps.stats[STAT_HEALTH]);
		lastRow = selected;
	}
}
