
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

#undef DEBUG
bool register_commands(LSPalmService *serviceHandle, LSError lserror);
const char 		*dbusAddress 		= "org.webosinternals.thmapi";
const char    *themes_base    = "/media/internal/.themes";
const char    *theme_link     = "/var/.theme";
char *theme = NULL;

//GHashTable *default_icons;
GMainLoop		*loop				= NULL;

bool luna_service_initialize() {

	bool retVal = FALSE;

	LSError lserror;
	LSErrorInit(&lserror);

	loop = g_main_loop_new(NULL, FALSE);
	if (loop==NULL)
		goto end;

	retVal = LSRegisterPalmService(dbusAddress, &serviceHandle, &lserror);
	if (retVal) {
#ifdef DEBUG
    printf("registered\n");
#endif
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

void luna_service_start() {

	//setup_event_callbacks();
  //default_icons = g_hash_table_new(g_str_hash, g_str_equal);
  //if (default_icons)
		g_main_loop_run(loop);

}

void luna_service_cleanup() {
  //if (default_icons)
    //g_hash_table_destroy(default_icons);
}

int main(int argc, char *argv[]) {

    if (luna_service_initialize())
            luna_service_start();

	return 0;

}

char *get_icon_path(const char *appid, const char *icon) {
  char *icon_path = NULL;
  FILE *fd = NULL;
  char png_header[8];
  const char *default_icon;
   
  if (!strncmp(icon, theme_link, strlen(theme_link)))
    default_icon = &icon[strlen(theme_link)];
  else
    default_icon = icon;

#ifdef DEBUG
  printf("get icon path for theme: %s\n", theme);
  printf("default icon %s\n", default_icon);
#endif

  if (theme) {
    asprintf(&icon_path, "%s%s", theme_link, default_icon);
    fd = fopen(icon_path, "rb");
    if (!fd) {
      free(icon_path);
      icon_path = NULL;
    }
    else {
      fread(png_header, 1, 8, fd);
      fclose(fd);

      if (png_sig_cmp(png_header, 0, 8)) {
        free(icon_path);
        icon_path = NULL;
      }
    }
  }

  if (!icon_path)
    icon_path = strdup(default_icon);

  return icon_path;
}

void set_launch_icon(const char *appid, const char *launchPointId, const char *icon) {
  LSError lserror;
  LSErrorInit(&lserror);
  const char *uri = "luna://com.palm.applicationManager/updateLaunchPointIcon";
  char *params = NULL;
  char *icon_path = NULL;
  char *from_app = NULL;

  icon_path = get_icon_path(appid, icon);
  if (icon_path) {
    asprintf(&params, "{\"launchPointId\":\"%s\", \"icon\":\"file://%s\"}", launchPointId, icon_path);
    asprintf(&from_app, "%s ", appid);

#ifdef DEBUG
    printf("update launch point! %s\n", icon_path);
#endif
    LSCallFromApplication(priv_serviceHandle, uri, params, from_app, NULL, NULL, NULL, &lserror);
  }
  
  if (icon_path) 
    free(icon_path);
  if (params)
    free(params);
  if (from_app) 
    free(from_app);
}

bool got_launch_points(LSHandle *lshandle, LSMessage *reply, void *ctx) {
  char *launch_points = NULL;
  char *icon_path = 0;
  char *launchPointId = 0;
  char *id = 0;
  char *icon = 0;
  int i = 0;
  int j = 0;

#ifdef DEBUG
  printf("got launch points %s\n", theme);
#endif
  json_t *object = LSMessageGetPayloadJSON(reply);
  if (!object) {
    return false;
  }

  json_t *myobj = json_find_first_label(object, "launchPoints");
  myobj=myobj->child->child;

  while (myobj) {
    json_get_string(myobj, "launchPointId", &launchPointId);
    json_get_string(myobj, "id", &id);
    json_get_string(myobj, "icon", &icon);
    myobj = myobj->next;
    icon_path = malloc(strlen(icon));
    i = j = 0;

    while (icon[i]) {
      if (icon[i] != '\\')
        icon_path[j++] = icon[i];

      i++;
    }

    icon_path[j]='\0';
    set_launch_icon(id, launchPointId, icon_path);
    free(icon_path);
  }

  LSError lserror;
  LSErrorInit(&lserror);
  const char *uri = "luna://com.palm.applicationManager/rescan";

  LSCall(priv_serviceHandle, uri, "{}", NULL, NULL, NULL, &lserror);

  return true;
}

bool currentTheme(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  char *jsonResponse = NULL;

  if (theme)
    asprintf(&jsonResponse, "{\"returnValue\":0, \"name\":\"%s\"}", theme);
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

bool setTheme(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  const char *uri = "luna://com.palm.applicationManager/listLaunchPoints";
  json_t *object = LSMessageGetPayloadJSON(message);
  char *tmp = NULL;

  json_get_string(object, "name", &tmp);

  if (theme) {
    free(theme);
    theme = NULL;
  }

  if (strlen(tmp) > 0) {
    theme = strdup(tmp);
  }

#ifdef DEBUG
  printf("update Icons to theme %s\n", theme);
#endif
  if (tmp) {
    unlink(theme_link);

    if (theme) {
      char *theme_dir = NULL;

      asprintf(&theme_dir, "%s/%s", themes_base, theme);
      symlink(theme_dir, theme_link);
      free(theme_dir);
    }

    LSCall(priv_serviceHandle, uri, "{}", got_launch_points, theme, NULL, &lserror);
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
