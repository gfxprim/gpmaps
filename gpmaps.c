//SPDX-License-Identifier: GPL-2.1-or-later

/*

    Copyright (C) 2021 Cyril Hrubis <metan@ucw.cz>

 */

#include <gfxprim.h>
#include "xqx.h"

static struct xqx_map *map;
static struct xqx_view *main_view;

static gp_widget *gps_error;

static void update_gps_status(struct xqx_gps_notify *nt,
                              enum xqx_gps_msg_type type, void *data)
{
	gp_widget *label = nt->priv;
	struct gps_fix_t *fix = data;

	switch (type) {
	case XQX_GPS_MSG_CONNECTED:
		gp_widget_label_set(label, "con");
	break;
	case XQX_GPS_MSG_DISCONNECTED:
		gp_widget_label_set(label, "discon");
	break;
	case XQX_GPS_MSG_NO_DATA:
		gp_widget_label_set(label, "con:no-data");
	break;
	case XQX_GPS_MSG_FIX:
		switch (fix->mode) {
		case MODE_NOT_SEEN:
		case MODE_NO_FIX:
			gp_widget_label_set(label, "con:no-fix");
		break;
		case MODE_2D:
			gp_widget_label_set(label, "con:2d-fix");
		break;
		case MODE_3D:
			gp_widget_label_set(label, "con:3d-fix");
		break;
		}

		if (gps_error) {
			double err = MAX(fix->epx, fix->epy);
			gp_widget_label_printf(gps_error, "err:%.2lfm", err);
		}
	break;
	}

}

static struct xqx_gps_notify widget_gps = {
	.gps_msg_cb = update_gps_status,
};

void *xqx_make_waypoints_layer(void*);

int zoom_in_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	xqx_view_zoom_in(main_view, 2<<10);

	return 0;
}

int zoom_out_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	xqx_view_zoom_out(main_view, 2<<10);

	return 0;
}

int move_up_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	xqx_view_move(main_view, 0, main_view->step_y);

	return 0;
}

int move_down_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	xqx_view_move(main_view, 0, -main_view->step_y);

	return 0;
}

int move_left_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	xqx_view_move(main_view, main_view->step_x, 0);

	return 0;
}

int move_right_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	xqx_view_move(main_view, -main_view->step_x, 0);

	return 0;
}

int main(int argc, char *argv[])
{
	gp_htable *uids;
	gp_widget *layout = gp_app_layout_load("gpmap", &uids);

	gp_widget *pixmap = gp_widget_by_uid(uids, "map_view", GP_WIDGET_PIXMAP);
	if (!pixmap)
		exit(1);

	widget_gps.priv = gp_widget_by_uid(uids, "gps_status", GP_WIDGET_LABEL);
	gps_error = gp_widget_by_uid(uids, "gps_error", GP_WIDGET_LABEL);

	if (widget_gps.priv)
		xqx_gps_register_notify(&widget_gps);

	gp_htable_free(uids);

	xqx_init();

	map = xqx_map_load("example-data/cz-osm-example/old.tmc");
	if (!map)
		printf("FAILED TO LOAD MAP\n");

	main_view = xqx_make_view(pixmap);

	main_view->maps = &map;
	main_view->maps_num = 1;
	xqx_view_choose_map(main_view, 0);

	xqx_view_enable_gps(main_view);

	struct xqx_path *path = xqx_path_geojson("example-data/waypoints.json");
	if (path) {
		xqx_path_print(path);
		xqx_view_prepend_layer(main_view, xqx_make_waypoints_layer(path));
	}

	gp_widgets_main_loop(layout, "gpmap", NULL, argc, argv);

	return 0;
}
