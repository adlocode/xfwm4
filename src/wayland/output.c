/* Copyright (c) 2018 - 2023 adlo
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "output.h"

void
output_frame_notify (struct wl_listener *listener, void *data)
{
    /* This function is called every time an output is ready to display a frame,
     * generally at the output's refresh rate (e.g. 60Hz). */
    Output *output = wl_container_of(listener, output, frame);
    struct wlr_scene *scene = output->server->scene;

    struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(
        scene, output->wlr_output);

    wlr_output_layout_get_box(output->server->output_layout,
            output->wlr_output, &output->geometry);

    /* Update the background for the current output size. */
    wlr_scene_rect_set_size(output->background,
            output->geometry.width, output->geometry.height);

    /* Render the scene if needed and commit the output */
    wlr_scene_output_commit(scene_output);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

void
output_destroy_notify (struct wl_listener *listener, void *data)
{
    Output *output = wl_container_of(listener, output, destroy);

    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->destroy.link);

    wl_list_remove(&output->request_state.link);


    /* Frees the layers */
    size_t num_layers = sizeof(output->layers) / sizeof(struct wlr_scene_node *);
    for (size_t i = 0; i < num_layers; i++)
    {
        struct wlr_scene_node *node =
            ((struct wlr_scene_node **) &output->layers)[i];
        wlr_scene_node_destroy(node);
    }

    wl_list_remove(&output->link);
    g_free(output);
}

void
new_output_notify (struct wl_listener *listener, void *data)
{
    /* This event is raised by the backend when a new output (aka a display or
     * monitor) becomes available. */
    xfwmWaylandCompositor *server =
        wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    /* Configures the output created by the backend to use our allocator
     * and our renderer. Must be done once, before commiting the output */
    wlr_output_init_render(wlr_output, server->allocator, server->renderer);

    /* Some backends don't have modes. DRM+KMS does, and we need to set a mode
     * before we can use the output. The mode is a tuple of (width, height,
     * refresh rate), and each monitor supports only a specific set of modes. We
     * just pick the monitor's preferred mode, a more sophisticated compositor
     * would let the user configure it. */
    if (!wl_list_empty(&wlr_output->modes))
    {
        struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
        wlr_output_set_mode(wlr_output, mode);
        wlr_output_enable(wlr_output, true);
        if (!wlr_output_commit(wlr_output))
        {
            return;
        }
    }

    /* Allocates and configures our state for this output */
    Output *output = g_new0 (Output, 1);
    output->wlr_output = wlr_output;
    output->server = server;

    /* Set the background color */
    float color[4] = {0.1875, 0.1875, 0.1875, 1.0};
    output->background = wlr_scene_rect_create(&server->scene->tree, 0, 0, color);
    wlr_scene_node_lower_to_bottom(&output->background->node);

    /* Initializes the layers */
    size_t num_layers = sizeof(output->layers) / sizeof(struct wlr_scene_node *);
    for (size_t i = 0; i < num_layers; i++)
    {
        ((struct wlr_scene_node **) &output->layers)[i] =
            &wlr_scene_tree_create(&server->scene->tree)->node;
    }

    /* Sets up a listener for the frame notify event. */
    output->frame.notify = output_frame_notify;
    wl_signal_add(&wlr_output->events.frame, &output->frame);

    /* Sets up a listener for the destroy notify event. */
    output->destroy.notify = output_destroy_notify;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    wl_list_insert(&server->outputs, &output->link);

    /* Adds this to the output layout. The add_auto function arranges outputs
     * from left-to-right in the order they appear. A more sophisticated
     * compositor would let the user configure the arrangement of outputs in the
     * layout.
     *
     * The output layout utility automatically adds a wl_output global to the
     * display, which Wayland clients can see to find out information about the
     * output (such as DPI, scale factor, manufacturer, etc).
     */
    wlr_output_layout_add_auto(server->output_layout, wlr_output);
}