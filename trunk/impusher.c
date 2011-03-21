/*
IM Pusher for Pidgin

Push messages to iOS devices

Copyright (C) 2011 WANG Lu <coolwanglu@gmail.com>
Copyright (C) 2010 Flavio Tischhauser <ftischhauser@gmail.com>
 
previously gtk-ftischhauser-notifo

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define PURPLE_PLUGINS
#define VERSION "0.0.7"

#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "notify.h"
#include "plugin.h"
#include "version.h"
#include "debug.h"
#include "cmds.h"
#include "gtkconv.h"
#include "prefs.h"
#include "gtkprefs.h"
#include "gtkutils.h"
#include "gtkplugin.h"
#include "gtkblist.h"

#define PLUGIN_ID "pidgin-im-pusher"
#define PLUGIN_NAME "IM Pusher"
#define PLUGIN_SUMMARY "Pushes IMs to your iOS devices."
#define PLUGIN_DESCRIPTION "Pushes IMs to your iOS devices, through notifo and/or pushme.to services."
#define PLUGIN_AUTHORS "Flavio Tischhauser  <ftischhauser@gmail.com>\nWANG Lu  <coolwanglu@gmail.com>"
#define PLUGIN_URL "http://code.google.com/p/pidgin-im-pusher/"

#define HTTP_USER_AGENT "Pidgin IM Pusher"

#define PREF_PREFIX "/plugins/gtk/"PLUGIN_ID

#define PREF_STATUS_PREFIX PREF_PREFIX"/status"

#define PREF_PUSH_PREFIX PREF_PREFIX"/push"

#define PREF_NOTIFO_PREFIX PREF_PUSH_PREFIX"/notifo"

#define PREF_NOTIFO_ENABLED PREF_NOTIFO_PREFIX"/enabled"
#define PREF_NOTIFO_USERNAME PREF_NOTIFO_PREFIX"/username"
#define PREF_NOTIFO_APIKEY PREF_NOTIFO_PREFIX"/apikey"

#define PREF_PUSHME_TO_PREFIX PREF_PUSH_PREFIX"/pushme.to"
#define PREF_PUSHME_TO_ENABLED PREF_PUSHME_TO_PREFIX"/enabled"
#define PREF_PUSHME_TO_USERNAME PREF_PUSHME_TO_PREFIX"/username"

// What we need in the configuration dialog
PurpleStatusPrimitive CONCERNED_STATUSES[] = {
    PURPLE_STATUS_AVAILABLE,
    PURPLE_STATUS_UNAVAILABLE,
    PURPLE_STATUS_INVISIBLE,
    PURPLE_STATUS_AWAY,
    PURPLE_STATUS_EXTENDED_AWAY
};

void received_IM (PurpleAccount *, char *, char *, PurpleMessageFlags, time_t);
void received_CM (PurpleAccount *, char *, char *, PurpleConversation *, PurpleMessageFlags);
static gboolean plugin_unload(PurplePlugin *);
static gboolean plugin_load(PurplePlugin *);
static void init_plugin(PurplePlugin *);
void notifo(const gchar *, const gchar *);
static void notifo_send_cb (PurpleUtilFetchUrlData *, gpointer, const gchar *, gsize, const gchar *);
void pushme_to(const gchar *, const gchar *);
static void pushme_to_send_cb (PurpleUtilFetchUrlData *, gpointer, const gchar *, gsize, const gchar *);
void main_CB(PurpleAccount *, char *, char *);


void notifo(const gchar *sender, const gchar *message)
{
    if(!purple_prefs_get_bool(PREF_NOTIFO_ENABLED))
        return;

	const char *username = purple_prefs_get_string(PREF_NOTIFO_USERNAME);
	const char *apikey = purple_prefs_get_string(PREF_NOTIFO_APIKEY);

	gchar *encsender = g_strdup(purple_url_encode(sender));
	gchar *encmsg = g_strdup(purple_url_encode(message));
	gchar *senddata = g_strdup_printf("label=Pidgin&title=%s&msg=%s", encsender, encmsg);
	gchar *authstring = g_strdup_printf("%s:%s", username, apikey);
	gchar *encauthstring = g_base64_encode(authstring, strlen(authstring));

	gchar *request = g_strdup_printf (
		"POST https://api.notifo.com/v1/send_notification HTTP/1.1\r\n"
		"User-Agent: "HTTP_USER_AGENT"\r\n"
		"Host: api.notifo.com\r\n"
		"Authorization: Basic %s\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: %d\r\n\r\n%s",
		encauthstring,
		strlen(senddata),
		senddata
		);

	g_free(encsender);
	g_free(encmsg);
	g_free(senddata);
	g_free(authstring);

	purple_util_fetch_url_request("https://api.notifo.com/v1/send_notification", TRUE, HTTP_USER_AGENT, TRUE, request, FALSE, notifo_send_cb, NULL);
	g_free(request);
}

void pushme_to(const gchar *sender, const gchar *message)
{
    if(!purple_prefs_get_bool(PREF_PUSHME_TO_ENABLED))
        return;

	const char *username = purple_prefs_get_string(PREF_PUSHME_TO_USERNAME);

    gchar *userurl = g_strdup_printf("http://pushme.to/%s/", username);
	gchar *encsender = g_strdup(purple_url_encode(sender));
	gchar *encmsg = g_strdup(purple_url_encode(message));
	gchar *senddata = g_strdup_printf("presentedName=%s&text=%s&filenames=", encsender, encmsg);
	gchar *request = g_strdup_printf (
		"POST http://pushme.to/%s/ HTTP/1.1\r\n"
		"User-Agent: "HTTP_USER_AGENT"\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: %d\r\n\r\n%s",
        username,
		strlen(senddata),
		senddata
		);

	g_free(encsender);
	g_free(encmsg);
	g_free(senddata);

	purple_util_fetch_url_request(userurl, TRUE, HTTP_USER_AGENT, TRUE, request, FALSE, pushme_to_send_cb, NULL);
    g_free(userurl);
	g_free(request);
}

static void notifo_send_cb (PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	//We don't really have anything to do in this callback function yet :)
}

static void pushme_to_send_cb (PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
}

void received_IM (PurpleAccount *account, char *sender, char *message, PurpleMessageFlags flags, time_t when)
{
	main_CB(account, sender, message);
}

void received_CM (PurpleAccount *account, char *sender, char *message, PurpleConversation *conv, PurpleMessageFlags flags)
{
	PurpleConvChat *chat = purple_conversation_get_chat_data(conv);

	if(flags & PURPLE_MESSAGE_NICK || purple_utf8_has_word(message, chat->nick))
		main_CB(account, sender, message);
}

void main_CB(PurpleAccount *account, char *sender, char *message)
{
    PurpleStatusPrimitive sp = purple_status_type_get_primitive(purple_status_get_type(purple_account_get_active_status(account)));
    gchar *path = g_strdup_printf(PREF_STATUS_PREFIX"/%d", sp);
    gboolean is_current_status_enabled = purple_prefs_get_bool(path);
    g_free(path);

    if(!is_current_status_enabled)
        return;

    if(purple_privacy_check(account, sender))
    {
        PurpleBuddy *buddy;
        buddy = purple_find_buddy(account, sender);
        const char *sender_name = (buddy != NULL) ? (purple_buddy_get_contact_alias(buddy)) : sender;

        notifo(sender_name, purple_markup_strip_html(message));
        pushme_to(sender_name, purple_markup_strip_html(message));
    }
}


static void init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none(PREF_PREFIX);

    purple_prefs_add_none(PREF_STATUS_PREFIX);
    PurpleStatusPrimitive sp;
    for(sp = 0; sp < PURPLE_STATUS_NUM_PRIMITIVES; ++sp)
    {
        gchar *path = g_strdup_printf(PREF_STATUS_PREFIX"/%d", sp);
        purple_prefs_add_bool(path, FALSE);
        g_free(path);
    }

	purple_prefs_add_none(PREF_PUSH_PREFIX);

	purple_prefs_add_none(PREF_NOTIFO_PREFIX);
    purple_prefs_add_bool(PREF_NOTIFO_ENABLED, FALSE);
	purple_prefs_add_string(PREF_NOTIFO_USERNAME, "");
	purple_prefs_add_string(PREF_NOTIFO_APIKEY, "");

	purple_prefs_add_none(PREF_PUSHME_TO_PREFIX);
    purple_prefs_add_bool(PREF_PUSHME_TO_ENABLED, FALSE);
	purple_prefs_add_string(PREF_PUSHME_TO_USERNAME, "");

}

static gboolean plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(purple_conversations_get_handle(), "received-im-msg", plugin, PURPLE_CALLBACK(received_IM), NULL);
	purple_signal_connect(purple_conversations_get_handle(), "received-chat-msg", plugin, PURPLE_CALLBACK(received_CM), NULL);
	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin) 
{
	purple_signal_disconnect(purple_conversations_get_handle(), "received-im-msg", plugin, PURPLE_CALLBACK(received_IM));
	purple_signal_disconnect(purple_conversations_get_handle(), "received-chat-msg", plugin, PURPLE_CALLBACK(received_CM));
	return TRUE;
}

static GtkWidget *plugin_config_frame(PurplePlugin *plugin) 
{
	GtkWidget *mainframe, *frame;
	GtkSizeGroup *sg;
    GtkVBox *vbox;

	mainframe= gtk_hbox_new(FALSE, 13);
    vbox = gtk_vbox_new(FALSE, 13);
    gtk_box_pack_start(mainframe, vbox, FALSE, FALSE, 13);

    frame = pidgin_make_frame(vbox, "Push when my status is:");
    int i;
    for(i = 0; i < sizeof(CONCERNED_STATUSES)/sizeof(PurpleStatusPrimitive); ++i)
    {
        PurpleStatusPrimitive sp = CONCERNED_STATUSES[i];
        gchar *path = g_strdup_printf(PREF_STATUS_PREFIX"/%d", sp);
        pidgin_prefs_checkbox(purple_primitive_get_name_from_type(sp), path, frame);
        // don't free it, as the controls need those pointers !
        // g_free(path);
    }

    vbox = gtk_vbox_new(FALSE, 13);
    gtk_box_pack_start(mainframe, vbox, FALSE, FALSE, 13);

    frame = pidgin_make_frame(vbox, "Notifo");
    pidgin_prefs_checkbox("Enabled", PREF_NOTIFO_ENABLED, frame);
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	pidgin_prefs_labeled_entry(frame,"Username:", PREF_NOTIFO_USERNAME,sg);
	pidgin_prefs_labeled_entry(frame,"API key:", PREF_NOTIFO_APIKEY,sg);

    frame = pidgin_make_frame(vbox, "Pushme.to");
    pidgin_prefs_checkbox("Enabled", PREF_PUSHME_TO_ENABLED, frame);
	pidgin_prefs_labeled_entry(frame,"Username:", PREF_PUSHME_TO_USERNAME,sg);

	gtk_widget_show_all(mainframe);
	return mainframe;
}

static PidginPluginUiInfo ui_info = {
	plugin_config_frame,
	0
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	PIDGIN_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
    PLUGIN_ID,
    PLUGIN_NAME,
	VERSION,
    PLUGIN_SUMMARY,
    PLUGIN_DESCRIPTION,
    PLUGIN_AUTHORS,
    PLUGIN_URL,
	plugin_load,
	plugin_unload,
	NULL,
	&ui_info,
	NULL,
	NULL,
	NULL
};

PURPLE_INIT_PLUGIN(IMPusher, init_plugin, info);
