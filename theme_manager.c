
/*=============================================================================
 Copyright (C) 2009 Ryan Hope <rmh3093@gmail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 =============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/timeb.h>
#include <string.h>
#include <glib.h>
#include <glib/gdir.h>
#include <semaphore.h>

#include <luna_service.h>

bool register_commands(LSPalmService *serviceHandle, LSError lserror);
const char 		*dbusAddress 		= "org.webosinternals.thmapi";
const char    *themes_base    = "/media/cryptofs/apps/.themes";
const char    *theme_link     = "/var/svc/org.webosinternals.thmapi/.theme";

typedef struct {
  char *id;
  char *launchPointId;
  char *default_icon;
  char *theme_icon;
} icon_t;

typedef struct theme {
  char *name;
  char *wallpaper;
  GHashTable *icons;
} theme_t;

theme_t myTheme;

GMainLoop		*loop				= NULL;

//GHashTable *test_icons;
bool luna_service_initialize() {

	bool retVal = FALSE;

	LSError lserror;
	LSErrorInit(&lserror);

	loop = g_main_loop_new(NULL, FALSE);
	if (loop==NULL)
		goto end;

	retVal = LSRegisterPalmService(dbusAddress, &serviceHandle, &lserror);
	if (retVal) {
		pub_serviceHandle = LSPalmServiceGetPublicConnection(serviceHandle);
		priv_serviceHandle = LSPalmServiceGetPrivateConnection(serviceHandle);
	} else
		goto end;

	register_commands(serviceHandle, lserror);
	//register_subscriptions(serviceHandle, lserror);

	LSGmainAttachPalmService(serviceHandle, loop, &lserror);

	end: if (LSErrorIsSet(&lserror)) {
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	return retVal;

}

void updateLaunchPointIcon(gpointer key, gpointer value, gpointer user_data) {
  LSError lserror;
  LSErrorInit(&lserror);
  icon_t *icon = (icon_t *)value;
  const char *uri = "luna://com.palm.applicationManager/updateLaunchPointIcon";
  char *launchPointId = (char *)key;
  char *iconPath = NULL;
  char *params = NULL;

  if (icon->theme_icon)
    asprintf(&iconPath, "%s/%s", theme_link, icon->theme_icon);
  else
    asprintf(&iconPath, "%s", icon->default_icon);

  if (iconPath) {
    asprintf(&params, "{\"launchPointId\":\"%s\", \"icon\":\"file://%s\"}", launchPointId, iconPath);
    LSCallFromApplication(priv_serviceHandle, uri, params, icon->id, NULL, NULL, NULL, &lserror);
  }

  if (iconPath)
    free(iconPath);
  if (params)
    free(params);
}

void clear_theme(gpointer key, gpointer value, gpointer user_data) {
  icon_t *icon = (icon_t *)value;

  if (icon->theme_icon) {
    free(icon->theme_icon);
    icon->theme_icon = NULL;
  }
}

bool setup_launch_icons(LSHandle *lshandle, LSMessage *reply, void *ctx) {
  char *hash_key = 0;
  char *launchPointId = 0;
  char *id = 0;
  char *iconPath = 0;
  static char *first_key = NULL;
  char *second_key = NULL;

  json_t *object = LSMessageGetPayloadJSON(reply);
  if (!object)
    return false;

  g_hash_table_foreach(myTheme.icons, clear_theme, NULL);

  json_t *myobj = json_find_first_label(object, "launchPoints");
  if (myobj && myobj->child && myobj->child->type == JSON_ARRAY) {
    myobj=myobj->child->child;
    while (myobj) {
      json_get_string(myobj, "launchPointId", &launchPointId);
      json_get_string(myobj, "id", &id);
      json_get_string(myobj, "icon", &iconPath);

      if (id && launchPointId && iconPath && !g_hash_table_lookup(myTheme.icons, launchPointId)) {
        icon_t *icon = calloc(1, sizeof(icon_t));

        asprintf(&icon->id, "%s ", id);
        icon->default_icon = strdup(iconPath);
        icon->launchPointId = strdup(launchPointId);
#ifdef DEBUG
        printf("INSERT HASH default icon %s, insert key %s: %s\n", id, launchPointId, iconPath);
#endif
        g_hash_table_insert(myTheme.icons, icon->launchPointId, icon);
      }

      myobj = myobj->next;
    }
  }

  if (myTheme.name) {
    get_theme_json();
  }

  g_hash_table_foreach(myTheme.icons, updateLaunchPointIcon, NULL);

  LSError lserror;
  LSErrorInit(&lserror);
  const char *uri = "luna://com.palm.applicationManager/rescan";

  LSCall(priv_serviceHandle, uri, "{}", NULL, NULL, NULL, &lserror);
}

void set_theme_name() {
  char *link = g_file_read_link(theme_link, NULL);
  char *theme = NULL;
  
  if (link) {
    theme = strrchr(link, '/');

    if (theme && theme[1])
      myTheme.name = strdup(theme + 1);

    free(link);
  }
}

void luna_service_start() {
  LSError lserror;
  LSErrorInit(&lserror);
  const char *uri = "luna://com.palm.applicationManager/listLaunchPoints";

  memset(&myTheme, 0, sizeof(myTheme));
  myTheme.icons = g_hash_table_new(g_str_hash, g_str_equal);
  set_theme_name();

  LSCall(priv_serviceHandle, uri, "{}", setup_launch_icons, NULL, NULL, &lserror);

  g_main_loop_run(loop);
}

void luna_service_cleanup() {
  //TODO clean up
}

int main(int argc, char *argv[]) {
  if (luna_service_initialize())
    luna_service_start();

	return 0;
}

bool currentTheme(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  char *jsonResponse = NULL;

  if (myTheme.name)
    asprintf(&jsonResponse, "{\"returnValue\":0, \"name\":\"%s\"}", myTheme.name);
  else
    asprintf(&jsonResponse, "{\"returnValue\":0, \"name\":\"\"}");

  LSMessageRespond(message, jsonResponse, &lserror);
}

bool getThemesList(LSHandle* lshandle, LSMessage *message, void *ctx) {
  const gchar *name = NULL;
  GDir *dir;
  char *jsonResponse = NULL; 
  char *iter = NULL;
  int first = 1;
  LSError lserror;
  LSErrorInit(&lserror);

  dir = g_dir_open((const char *)themes_base, 0, NULL);

  if (!dir) {
    LSMessageRespond(message, "{\"returnValue\":-1, \"errorText\":\"Cannot open themes dir\"}", &lserror);
    return false;
  }

  asprintf(&jsonResponse, "{\"list\": [");

  while (name = g_dir_read_name(dir)) {
    if (first)
      asprintf(&jsonResponse, "%s\"%s\"", jsonResponse, name);
    else
      asprintf(&jsonResponse, "%s,\"%s\"", jsonResponse, name);

    first = 0;
  }

  asprintf(&jsonResponse, "%s]}", jsonResponse);
  LSMessageRespond(message, jsonResponse, &lserror);

  g_dir_close(dir);
  free(jsonResponse);
  return true;
}

int get_theme_json() {
  FILE *fd;
  char *filename;
  gchar *jsonObject = NULL;
  gsize length = 0;
  json_t *root;

  asprintf(&filename, "%s/theme_config.json", theme_link);
  g_file_get_contents(filename, &jsonObject, &length, NULL);
  
  if (jsonObject) {
    //printf("GOT json %s\n", jsonObject);
  }

  root = json_parse_document(jsonObject);
  json_t *entry = json_find_first_label(root, "wallpaper");

  if (entry)
    json_get_string(entry->child, "image", &myTheme.wallpaper);

  entry = json_find_first_label(root, "applications");
  if (entry && entry->child && entry->child->type == JSON_ARRAY) {
    entry = entry->child->child;
    while (entry) {
      char *appId = NULL;
      char *launchPointId = NULL;
      char *iconPath = NULL;
      char *hash_key = NULL;
      icon_t *icon;
      json_get_string(entry, "appId", &appId);
      json_get_string(entry, "launchPointId", &launchPointId);
      json_get_string(entry, "icon", &iconPath);
      if (appId && iconPath) {
        if (launchPointId)
          hash_key = strdup(launchPointId);
        else
          asprintf(&hash_key, "%s_default", appId);

        icon = g_hash_table_lookup(myTheme.icons, hash_key);

        if (icon) {
          if (icon->theme_icon && strcmp(iconPath, icon->theme_icon)) {
            free(icon->theme_icon);
            icon->theme_icon = NULL;
          }
          if (!icon->theme_icon) {
            icon->theme_icon = strdup(iconPath);
            g_hash_table_insert(myTheme.icons, hash_key, icon);
          }
        }
        else {
          printf("ERROR: could not find hash for %s\n", hash_key);
        }

        free(hash_key);
      }
      entry = entry->next;
    }
  }

  if (jsonObject)
    free(jsonObject);
  if (filename) 
    free(filename);

  return 0;
}

bool setTheme(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  const char *uri = "luna://com.palm.applicationManager/listLaunchPoints";
  json_t *object = LSMessageGetPayloadJSON(message);
  char *tmp = NULL;

  json_get_string(object, "name", &tmp);

  if (myTheme.name) {
    free(myTheme.name);
    myTheme.name = NULL;
  }

  if (tmp && strlen(tmp) > 0) {
    myTheme.name = strdup(tmp);
  }

  if (tmp) {
    unlink(theme_link);

    if (myTheme.name) {
      char *theme_dir = NULL;

      asprintf(&theme_dir, "%s/%s", themes_base, myTheme.name);
      symlink(theme_dir, theme_link);
      free(theme_dir);
    }

    LSCall(priv_serviceHandle, uri, "{}", setup_launch_icons, NULL, NULL, &lserror);
    LSMessageRespond(message,"{\"returnValue\":0}",&lserror);
  }
  else {
    LSMessageRespond(message,"{\"returnValue\":-1, \"errorText\":\"Must supply theme name\"}",&lserror);
  }
}

LSMethod lscommandmethods[] = {
		{"setTheme", setTheme},
    {"getThemesList", getThemesList},
    {"currentTheme", currentTheme},
		{0,0}
};

bool register_commands(LSPalmService *serviceHandle, LSError lserror) {
	return LSPalmServiceRegisterCategory(serviceHandle, "/", lscommandmethods,
			NULL, NULL, NULL, &lserror);
}
