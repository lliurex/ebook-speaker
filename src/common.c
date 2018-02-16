/* common.c - common functions used by daisy-player and eBook-speaker.
 *
 * Copyright (C)2018 J. Lemmens
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "daisy.h"

void get_volume (misc_t *misc)
{
   char dev[MAX_STR + 10];
   snd_mixer_t *handle;
   snd_mixer_selem_id_t *sid;
   snd_mixer_elem_t *elem;

   if (snd_mixer_open (&handle, atoi (misc->sound_dev)) != 0)
      failure (misc, "snd_mixer_open", errno);
   sprintf (dev, "hw:%s", misc->sound_dev);
   snd_mixer_attach (handle, dev);
   snd_mixer_selem_register (handle, NULL, NULL);
   snd_mixer_load (handle);
   snd_mixer_selem_id_alloca (&sid);
   snd_mixer_selem_id_set_index (sid, 0);
   snd_mixer_selem_id_set_name (sid, "Master");
   if ((elem = snd_mixer_find_selem (handle, sid)) == NULL)
      failure (misc, "No ALSA device found\n", errno);
   snd_mixer_selem_get_playback_volume_range (elem,
      &misc->min_vol, &misc->max_vol);
   snd_mixer_selem_get_playback_volume (elem, atoi (misc->sound_dev), &misc->volume);
   snd_mixer_close (handle);
} // get_volume

void set_volume (misc_t *misc)
{
   char dev[MAX_STR + 10];
   snd_mixer_t *handle;
   snd_mixer_selem_id_t *sid;
   snd_mixer_elem_t *elem;

   if (snd_mixer_open (&handle, atoi (misc->sound_dev)) != 0)
      failure (misc, "snd_mixer_open", errno);
   sprintf (dev, "hw:%s", misc->sound_dev);
   snd_mixer_attach (handle, dev);
   snd_mixer_selem_register (handle, NULL, NULL);
   snd_mixer_load (handle);
   snd_mixer_selem_id_alloca (&sid);
   snd_mixer_selem_id_set_index (sid, 0);
   snd_mixer_selem_id_set_name (sid, "Master");
   if ((elem = snd_mixer_find_selem (handle, sid)) == NULL)
      failure (misc, "No ALSA device found", errno);
   snd_mixer_selem_set_playback_volume_all (elem, misc->volume);
   snd_mixer_close (handle);
} // set_volume

char *convert_URL_name (misc_t *misc, char *file)
{
   int i, j;

   *misc->str = j = 0;
   for (i = 0; i < (int) strlen (file) - 2; i++)
   {
      if (file[i] == '%' && isxdigit (file[i + 1]) && isxdigit (file[i + 2]))
      {
         char hex[10];

         hex[0] = '0';
         hex[1] = 'x';
         hex[2] = file[i + 1];
         hex[3] = file[i + 2];
         hex[4] = 0;
         sscanf (hex, "%x", (unsigned int *) &misc->str[j++]);
         i += 2;
      }
      else
         misc->str[j++] = file[i];
   } // for
   misc->str[j++] = file[i++];
   misc->str[j++] = file[i];
   misc->str[j] = 0;
   get_path_name (misc, misc->daisy_mp, misc->str);
   return misc->path_name;
} // convert_URL_name

void failure (misc_t *misc, char *str, int e)
{
   endwin ();
   beep ();
   fprintf (stderr, "\n\n%s: %s\n", str, strerror (e));
   fflush (stdout);
   remove_tmp_dir (misc);
   _exit (-1);
} // failure

void playfile (misc_t *misc, char *in_file, char *in_type,
               char *out_file, char *out_type, char *tempo)
{
   char *cmd;

   fclose (stdin);
   fclose (stdout);
   fclose (stderr);
#ifdef DAISY_PLAYER
   if (strcmp (in_type, "cdda") == 0)
   {
      cmd = malloc (strlen (in_type) + strlen (in_file) +
                    strlen (out_type) + strlen (out_file) + 50);
      sprintf (cmd, "sox -t cdda -L \"%s\" -t %s \"%s\" tempo -m %s",
               in_file, out_type, out_file, tempo);
   }
   else
#endif
   {
      cmd = malloc (strlen (in_type) + strlen (in_file) +
                    strlen (out_type) + strlen (out_file) + 50);
      sprintf (cmd, "sox -t %s \"%s\" -t %s \"%s\" tempo -s %s",
               in_type, in_file, out_type, out_file, tempo);
   } // if
   switch (system (cmd));
   free (cmd);
   unlink (in_file);
   unlink (misc->tmp_wav);
} // playfile

void player_ended ()
{
   wait (NULL);
} // player_ended

void get_path_name (misc_t *misc, char *dir, char *name)
{
   int total, n;
   struct dirent **namelist;

   total = scandir (dir, &namelist, NULL, alphasort);
   if (total == -1)
   {
      printf ("scandir: %s: %s\n", dir, strerror (errno));
      _exit (-1);
   } // if
   for (n = 0; n < total; n++)
   {
      char *path;

      if (strcmp (namelist[n]->d_name, ".") == 0 ||
          strcmp (namelist[n]->d_name, "..") == 0)
         continue;
      path = malloc (strlen (dir) + strlen (namelist[n]->d_name) + 5);
      if (dir[strlen (dir) - 1] == '/')
         sprintf (path, "%s%s", dir, namelist[n]->d_name);
      else
         sprintf (path, "%s/%s", dir, namelist[n]->d_name);
      if (strcasestr (path, name))
      {
         misc->path_name = strdup (path);
         return;
      } // if
      free (path);
      if (namelist[n]->d_type == DT_DIR)
      {
         char *new_dir;

         new_dir = malloc (strlen (dir) + strlen (namelist[n]->d_name) + 5);
         if (dir[strlen (dir) - 1] != '/')
            sprintf (new_dir, "%s/%s", dir, namelist[n]->d_name);
         else
            sprintf (new_dir, "%s%s", dir, namelist[n]->d_name);
         get_path_name (misc, new_dir, name);
         free (new_dir);
      } // if
   } // for
   free (namelist);
} // get_path_name

void find_index_names (misc_t *misc)
{
   misc->path_name = strdup ("");
   *misc->ncc_html = 0;
   get_path_name (misc, misc->daisy_mp, "ncc.html");
   strncpy (misc->ncc_html, misc->path_name, MAX_STR - 1);
   misc->path_name = strdup ("");
   *misc->ncx_name = 0;
   get_path_name (misc, misc->daisy_mp, ".ncx");
   strncpy (misc->ncx_name, misc->path_name, MAX_STR - 1);
   *misc->opf_name = 0;
   get_path_name (misc, misc->daisy_mp, ".opf");
   strncpy (misc->opf_name, misc->path_name, MAX_STR - 1);
} // find_index_names

int get_page_number_2 (misc_t *misc, my_attribute_t *my_attribute,
                       daisy_t *daisy, char *attr)
{
// function for daisy 2.02
   if (daisy[misc->playing].page_number == 0)
      return 0;
#ifdef DAISY_PLAYER
   char *src, *anchor;
   xmlTextReaderPtr page;
   htmlDocPtr doc;

   src = malloc (strlen (misc->daisy_mp) + strlen (attr) + 3);
   strcpy (src, misc->daisy_mp);
   strcat (src, "/");
   strcat (src, attr);
   anchor = strdup ("");
   if (strchr (src, '#'))
   {
      anchor = strdup (strchr (src, '#') + 1);
      *strchr (src, '#') = 0;
   } // if
   doc = htmlParseFile (src, "UTF-8");
   if (! (page = xmlReaderWalker (doc)))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, gettext ("Cannot read %s"), src);
      failure (misc, str, e);
   } // if
   if (*anchor)
   {
      do
      {
         if (! get_tag_or_label (misc, my_attribute, page))
         {
            xmlTextReaderClose (page);
            xmlFreeDoc (doc);
            return 0;
         } // if
      } while (strcasecmp (my_attribute->id, anchor) != 0);
   } // if
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, page))
      {
         xmlTextReaderClose (page);
         xmlFreeDoc (doc);
         return 0;
      } // if
      if (*misc->label)
      {
         xmlTextReaderClose (page);
         xmlFreeDoc (doc);
         misc->current_page_number = atoi (misc->label);
         return 1;
      } // if
   } // while
#endif

#ifdef EBOOK_SPEAKER
   while (1)
   {
      if (*misc->label)
      {
         misc->current_page_number = atoi (misc->label);
         return 1;
      } // if
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         return 0;
   } // while
   attr = attr; // don't need it in eBook-speaker
#endif
} // get_page_number_2

int get_page_number_3 (misc_t *misc, my_attribute_t *my_attribute)
{
// function for daisy 3
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         return 0;
#ifdef DAISY_PLAYER
      if (strcasecmp (misc->tag, "text") == 0)
      {
         char *file, *anchor;
         xmlTextReaderPtr page;
         htmlDocPtr doc;

         anchor = strdup ("");
         if (strchr (my_attribute->src, '#'))
         {
            anchor = strdup (strchr (my_attribute->src, '#') + 1);
            *strchr (my_attribute->src, '#') = 0;
         } // if
         file = convert_URL_name (misc, my_attribute->src);
         doc = htmlParseFile (file, "UTF-8");
         if (! (page = xmlReaderWalker (doc)))
         {
            int e;
            char str[MAX_STR];

            e = errno;
            snprintf (str, MAX_STR, gettext ("Cannot read %s"), file);
            failure (misc, str, e);
         } // if
         if (*anchor)
         {
            do
            {
               if (! get_tag_or_label (misc, my_attribute, page))
               {
                  xmlTextReaderClose (page);
                  xmlFreeDoc (doc);
                  return 0;
               } // if
            } while (strcasecmp (my_attribute->id, anchor) != 0);
         } // if anchor
         while (1)
         {
            if (! get_tag_or_label (misc, my_attribute, page))
            {
               xmlTextReaderClose (page);
               xmlFreeDoc (doc);
               return 0;
            } // if
            if (*misc->label)
            {
               xmlTextReaderClose (page);
               xmlFreeDoc (doc);
               misc->current_page_number = atoi (misc->label);
               return 1;
            } // if
         } // while
      } // if text
#endif

#ifdef EBOOK_SPEAKER
      if (*misc->label)
      {
         misc->current_page_number = atoi (misc->label);
         return 1;
      } // if
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         return 0;
#endif
   } // while
} // get_page_number_3                                         

void kill_player (misc_t *misc)
{
   while (killpg (misc->player_pid, SIGHUP) == 0);
#ifdef EBOOK_SPEAKER
   unlink (misc->eBook_speaker_txt);
   unlink (misc->tmp_wav);
#endif
#ifdef DAISY_PLAYER
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      while (killpg (misc->cdda_pid, SIGKILL) == 0);
   unlink (misc->tmp_wav);
#endif
} // kill_player

void skip_right (misc_t *misc, daisy_t *daisy)
{
#ifdef DAISY_PLAYER
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      return;
#endif
   if (misc->playing == -1)
   {
      beep ();
      return;
   } // if
   misc->current = misc->displaying = misc->playing;
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   kill_player (misc);
} // skip_right

int handle_ncc_html (misc_t *misc, my_attribute_t *my_attribute)
{
// lookfor "ncc.html"
   htmlDocPtr doc;
   xmlTextReaderPtr ncc;

   misc->daisy_mp = strdup (misc->ncc_html);
   misc->daisy_mp = dirname (misc->daisy_mp);
   strncpy (misc->daisy_version, "2.02", 4);
   doc = htmlParseFile (misc->ncc_html, "UTF-8");
   ncc = xmlReaderWalker (doc);
   misc->total_items = 0;
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, ncc))
         break;
      if (strcasecmp (misc->tag, "h1") == 0 ||
          strcasecmp (misc->tag, "h2") == 0 ||
          strcasecmp (misc->tag, "h3") == 0 ||
          strcasecmp (misc->tag, "h4") == 0 ||
          strcasecmp (misc->tag, "h5") == 0 ||
          strcasecmp (misc->tag, "h6") == 0)
      {
         misc->total_items++;
      } // if
   } // while
   xmlTextReaderClose (ncc);
   xmlFreeDoc (doc);
   return misc->total_items;
} // handle_ncc_html

int namefilter (const struct dirent *namelist)
{
   int  r = 0;
   char     my_pattern[] = "*.smil";

   r = fnmatch (my_pattern, namelist->d_name, FNM_PERIOD);
   return (r == 0) ? 1 : 0;
} // namefilter

int get_meta_attributes (xmlTextReaderPtr parse, xmlTextWriterPtr writer)
{
   while (1)
   {
      char *name, *content;

      name = (char *) xmlTextReaderGetAttribute (parse,
                            (const unsigned char *) "name");
      if (! name)
         return 0;
      content = (char *) xmlTextReaderGetAttribute (parse,
                     (const unsigned char *) "content");
      if (strcasecmp (name, "title") == 0)
      {
         xmlTextWriterWriteString (writer, (const xmlChar *)content);
         return 1;
      } // if
      if (xmlTextReaderRead (parse) == 0)
         return 0;
      if (xmlTextReaderRead (parse) == 0)
         return 0;
   } // while
} // get_meta_attributes

void create_ncc_html (misc_t *misc)
{
   xmlTextWriterPtr writer;
   int total, n;
   struct dirent **namelist;

   sprintf (misc->ncc_html, "%s/ncc.html", misc->daisy_mp);
   if (! (writer = xmlNewTextWriterFilename (misc->ncc_html, 0)))
      failure (misc, misc->ncc_html, errno);
   xmlTextWriterSetIndent (writer, 1);
   xmlTextWriterSetIndentString (writer, BAD_CAST "   ");
   xmlTextWriterStartDocument (writer, NULL, NULL, NULL);
   xmlTextWriterStartElement (writer, BAD_CAST "html");
   xmlTextWriterWriteString (writer, BAD_CAST "\n");
   xmlTextWriterStartElement (writer, BAD_CAST "head");
   xmlTextWriterWriteString (writer, BAD_CAST "\n   ");
   xmlTextWriterEndElement (writer);
   xmlTextWriterStartElement (writer, BAD_CAST "body");
   xmlTextWriterWriteString (writer, (const xmlChar *)"\n");
   total = scandir (get_current_dir_name (), &namelist, namefilter,
                    alphasort);
   for (n = 0; n < total; n++)
   {
      xmlTextWriterStartElement (writer, BAD_CAST "h1");
      xmlTextWriterWriteString (writer, BAD_CAST "\n");
      xmlTextWriterStartElement (writer, BAD_CAST "a");
      xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "href",
                                         "%s", namelist[n]->d_name);

// write label
      {
      xmlTextReaderPtr parse;
      char *str, *tag;

      str = malloc (strlen (misc->daisy_mp) + strlen (namelist[n]->d_name) + 5);
      sprintf (str, "%s/%s", misc->daisy_mp, namelist[n]->d_name);
      parse = xmlReaderForFile (str, "UTF-8", 0);
      while (1)
      {
         if (xmlTextReaderRead (parse) == 0)
            break;
         tag = (char *) xmlTextReaderConstName (parse);
         if (strcasecmp (tag, "/head") == 0)
            break;
         if (strcasecmp (tag, "body") == 0)
            break;
         if (strcasecmp (tag, "meta") == 0)
         {
            if (get_meta_attributes (parse, writer))
               break;
         } // if
      } // while
      xmlTextReaderClose (parse);
      free (str);
      } // write label

      xmlTextWriterWriteString (writer, BAD_CAST "\n         ");
      xmlTextWriterEndElement (writer);
      xmlTextWriterEndElement (writer);
   } // for
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndDocument (writer);
   xmlFreeTextWriter (writer);
   free (namelist);
} // create_ncc_html

#ifdef DAISY_PLAYER
daisy_t *create_daisy_struct (misc_t *misc, my_attribute_t *my_attribute,
                              daisy_t *daisy)
#endif
#ifdef EBOOK_SPEAKER
daisy_t *create_daisy_struct (misc_t *misc, my_attribute_t *my_attribute)
#endif
{
   htmlDocPtr doc;
   xmlTextReaderPtr ptr;

   misc->total_pages = 0;
   *misc->daisy_version = 0;\
   *misc->ncc_html = *misc->ncx_name = *misc->opf_name = 0;
   find_index_names (misc);

// lookfor "ncc.html"
   if (*misc->ncc_html)
   {
      misc->total_items = handle_ncc_html (misc, my_attribute);
      return (daisy_t *) malloc ((misc->total_items + 1) * sizeof (daisy_t));
   } // if ncc.html

// lookfor *.ncx
   if (*misc->ncx_name)
      strncpy (misc->daisy_version, "3", 2);
   if (strlen (misc->ncx_name) < 4)
      *misc->ncx_name = 0;

// lookfor "*.opf""
   if (*misc->opf_name)
      strncpy (misc->daisy_version, "3", 2);
   if (strlen (misc->opf_name) < 4)
      *misc->opf_name = 0;

   if (*misc->ncc_html == 0 && *misc->ncx_name == 0 && *misc->opf_name == 0)
   {
      create_ncc_html (misc);
      misc->total_items = handle_ncc_html (misc, my_attribute);
      return (daisy_t *) malloc ((misc->total_items + 1) * sizeof (daisy_t));
   } // if

// count items in opf
   misc->items_in_opf = 0;
   doc = htmlParseFile (misc->opf_name, "UTF-8");
   ptr = xmlReaderWalker (doc);
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, ptr))
         break;
      if (strcasecmp (misc->tag, "itemref") == 0)
         misc->items_in_opf++;
   } // while
   xmlTextReaderClose (ptr);
   xmlFreeDoc (doc);

// count items in ncx
   misc->items_in_ncx = 0;
   doc = htmlParseFile (misc->ncx_name, "UTF-8");
   ptr = xmlReaderWalker (doc);
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, ptr))
         break;
      if (strcasecmp (misc->tag, "navpoint") == 0)
         misc->items_in_ncx++;
   } // while
   xmlTextReaderClose (ptr);
   xmlFreeDoc (doc);

   if (misc->items_in_ncx == 0 && misc->items_in_opf == 0)
   {
      endwin ();
      printf ("%s\n",
              gettext ("Please try to play this book with daisy-player"));
      beep ();
#ifdef DAISY_PLAYER
      quit_daisy_player (misc, daisy);
#endif
#ifdef EBOOK_SPEAKER
      quit_eBook_speaker (misc);
#endif
      _exit (0);
   } // if

   misc->total_items = misc->items_in_ncx;
   if (misc->items_in_opf > misc->items_in_ncx)
      misc->total_items = misc->items_in_opf;
   switch (chdir (misc->daisy_mp));
#ifdef EBOOK_SPEAKER
   snprintf (misc->eBook_speaker_txt, MAX_STR,
             "%s/eBook-speaker.txt", misc->daisy_mp);
   snprintf (misc->tmp_wav, MAX_STR,
             "%s/eBook-speaker.wav", misc->daisy_mp);
#endif
   if (misc->total_items == 0)
      misc->total_items = 1;
   return (daisy_t *) malloc (misc->total_items * sizeof (daisy_t));
} // create_daisy_struct

void make_tmp_dir (misc_t *misc)
{
   misc->tmp_dir = strdup ("/tmp/" PACKAGE ".XXXXXX");
   if (! mkdtemp (misc->tmp_dir))
   {
      int e;

      e = errno;
      beep ();
      failure (misc, misc->tmp_dir, e);
   } // if      
#ifdef EBOOK_SPEAKER
   switch (chdir (misc->tmp_dir))
   {
   case 01:
      failure (misc, misc->tmp_dir, errno);
   default:
      break;
   } // switch
#endif
} // make_tmp_dir

void remove_tmp_dir (misc_t *misc)
{
   if (strncmp (misc->tmp_dir, "/tmp/", 5) == 0)
   {
// Be sure not to remove wrong files
#ifdef DAISY_PLAYER
      if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      {
         rmdir (misc->tmp_dir);
         return;
      } // if
#endif

      snprintf (misc->cmd, MAX_CMD - 1, "rm -rf %s", misc->tmp_dir);
      if (system (misc->cmd) != 0)
      {
         int e;
         char *str;

         e = errno;
         str = malloc (strlen (misc->cmd) + strlen (strerror (e)) + 10);
         sprintf (str, "%s: %s\n", misc->cmd, strerror (e));
         failure (misc, str, e);
      } // if
   } // if
} // remove_tmp_dir               

void clear_tmp_dir (misc_t *misc)
{
   if (strstr (misc->tmp_dir, "/tmp/"))
   {
// Be sure not to remove wrong files
      snprintf (misc->cmd, MAX_CMD - 1, "rm -rf %s/*", misc->tmp_dir);
      switch (system (misc->cmd));
   } // if
} // clear_tmp_dir

void get_attributes (misc_t *misc, my_attribute_t *my_attribute,
                     xmlTextReaderPtr ptr)
{
   char attr[MAX_STR + 1];

   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "class"));
   if (strcmp (attr, "(null)"))
      snprintf (my_attribute->class, MAX_STR - 1, "%s", attr);
#ifdef EBOOK_SPEAKER
   if (misc->option_b == 0)
   {
      snprintf (attr, MAX_STR - 1, "%s", (char *)
                xmlTextReaderGetAttribute
                               (ptr, (xmlChar *) "break_on_eol"));
      if (strcmp (attr, "(null)"))
      {
         misc->break_on_EOL = attr[0];
         if (misc->break_on_EOL != 'y' && misc->break_on_EOL != 'n')
            misc->break_on_EOL = 'n';
      } // if
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "my_class"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->my_class, attr, MAX_STR - 1);
#endif
#ifdef DAISY_PLAYER
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "clip-begin"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_begin, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "clipbegin"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_begin, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "clip-end"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_end, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "clipend"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_end, attr, MAX_STR - 1);
#endif
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "href"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->href, attr, MAX_STR - 1);
   if (xmlTextReaderGetAttribute (ptr, (const xmlChar *) "id") != NULL)
   {
      my_attribute->id = strdup ((char *) xmlTextReaderGetAttribute (ptr,
               (const xmlChar *) "id"));
#ifdef DAISY_PLAYER
      if (strcmp (my_attribute->id, misc->current_id) != 0)
         misc->current_id = strdup (my_attribute->id);
#endif
   } // if
   if (xmlTextReaderGetAttribute (ptr, (const xmlChar *) "idref") != NULL)
   {                  
      my_attribute->idref = strdup ((char *) xmlTextReaderGetAttribute (ptr,
               (const xmlChar *) "idref"));
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "item"));
   if (strcmp (attr, "(null)"))
      misc->current = atoi (attr);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "level"));
   if (strcmp (attr, "(null)"))
      misc->level = atoi ((char *) attr);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
          xmlTextReaderGetAttribute (ptr, (const xmlChar *) "media-type"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->media_type, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "name"));
   if (strcmp (attr, "(null)"))
   {
      char name[MAX_STR], content[MAX_STR];

      *name = 0;
      strncpy (attr, (char *) xmlTextReaderGetAttribute
               (ptr, (const xmlChar *) "name"), MAX_STR - 1);
      if (strcmp (attr, "(null)"))
         strncpy (name, attr, MAX_STR - 1);
      *content = 0;
      snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "content"));
      if (strcmp (attr, "(null)"))
         strncpy (content, attr, MAX_STR - 1);
      if (strcasestr (name, "dc:format"))
         strncpy (misc->daisy_version, content, MAX_STR - 1);
      if (strcasestr (name, "dc:title") && ! *misc->daisy_title)
      {
         strncpy (misc->daisy_title, content, MAX_STR - 1);
         strncpy (misc->bookmark_title, content, MAX_STR - 1);
         if (strchr (misc->bookmark_title, '/'))
            *strchr (misc->bookmark_title, '/') = '-';
      } // if
/* don't use it!!!
      if (strcasestr (name, "dtb:misc->depth"))
         misc->depth = atoi (content);
*/
      if (strcasestr (name, "dtb:totalPageCount"))
         misc->total_pages = atoi (content);
/* don't use it!!!
      if (strcasestr (name, "ncc:misc->depth"))
         misc->depth = atoi (content);
*/
      if (strcasestr (name, "ncc:maxPageNormal"))
         misc->total_pages = atoi (content);
      if (strcasestr (name, "ncc:pageNormal"))
         misc->total_pages = atoi (content);
      if (strcasestr (name, "ncc:page-normal"))
         misc->total_pages = atoi (content);
      if (strcasestr (name, "dtb:totalTime")  ||
          strcasestr (name, "ncc:totalTime"))
      {
         strncpy (misc->ncc_totalTime, content, MAX_STR - 1);
         if (strchr (misc->ncc_totalTime, '.'))
            *strchr (misc->ncc_totalTime, '.') = 0;
      } // if
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
           xmlTextReaderGetAttribute (ptr, (const xmlChar *) "playorder"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->playorder, attr, MAX_STR - 1);
#ifdef EBOOK_SPEAKER
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "phrase"));
   if (strcmp (attr, "(null)"))
      misc->phrase_nr = atoi ((char *) attr);
#endif
#ifdef DAISY_PLAYER
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "seconds"));
   if (strcmp (attr, "(null)"))
   {
      misc->seconds = atoi (attr);
      if (misc->seconds < 0)
         misc->seconds = 0;
   } // if
#endif
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "smilref"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->smilref, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
           xmlTextReaderGetAttribute (ptr, (const xmlChar *) "sound_dev"));
   if (strcmp (attr, "(null)"))
      misc->sound_dev = strdup (attr);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
        xmlTextReaderGetAttribute (ptr, (const xmlChar *) "ocr_language"));
   if (strcmp (attr, "(null)"))
      strncpy (misc->ocr_language, attr, 5);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "cd_dev"));
   if (strcmp (attr, "(null)"))
      strncpy (misc->cd_dev, attr, MAX_STR - 1);
#ifdef DAISY_PLAYER
   snprintf (attr, MAX_STR - 1, "%s", (char *)
           xmlTextReaderGetAttribute (ptr, (const xmlChar *) "cddb_flag"));
   if (strcmp (attr, "(null)"))
      misc->cddb_flag = (char) attr[0];
#endif      
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "speed"));
   if (strcmp (attr, "(null)"))
   {
      misc->speed = atof (attr);
      if (misc->speed <= 0.1 || misc->speed > 2)
         misc->speed = 1.0;
   } // if
   if (xmlTextReaderGetAttribute (ptr, (const xmlChar *) "src") != NULL)
   {
      my_attribute->src = strdup ((char *) xmlTextReaderGetAttribute (ptr,
               (const xmlChar *) "src"));
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "tts"));
   if (strcmp (attr, "(null)"))
      misc->tts_no = atof ((char *) attr);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "toc"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->toc, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "value"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->value, attr, MAX_STR - 1);
} // get_attributes

int get_tag_or_label (misc_t *misc, my_attribute_t *my_attribute,
                      xmlTextReaderPtr reader)
{
   int type;

   *misc->tag = 0;
   misc->label = strdup ("");
#ifdef EBOOK_SPEAKER
   *my_attribute->my_class = 0;
#endif
   *my_attribute->class = 0;
#ifdef DAISY_PLAYER
   *my_attribute->clip_begin = *my_attribute->clip_end = 0;
#endif
   *my_attribute->href = *my_attribute->media_type =
   *my_attribute->playorder = *my_attribute->smilref =
   *my_attribute->toc = *my_attribute->value = 0;
   my_attribute->id = strdup ("");
   my_attribute->idref = strdup ("");
   my_attribute->src = strdup ("");

   if (! reader)
   {
      return 0;
   } // if
   switch (xmlTextReaderRead (reader))
   {
   case -1:
   {
      failure (misc, "xmlTextReaderRead ()\n"
               "Can't handle this DTB structure!\n"
               "Don't know how to handle it yet, sorry. (-:\n", errno);
      break;
   }
   case 0:
   {
      return 0;
   }
   case 1:
      break;
   } // swwitch
   type = xmlTextReaderNodeType (reader);
   switch (type)
   {
   int n, i;

   case -1:
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, gettext ("Cannot read type: %d"), type);
      failure (misc, str, e);
      break;
   }
   case XML_READER_TYPE_ELEMENT:
      strncpy (misc->tag, (char *) xmlTextReaderConstName (reader),
               MAX_TAG - 1);
      n = xmlTextReaderAttributeCount (reader);
      for (i = 0; i < n; i++)
         get_attributes (misc, my_attribute, reader);
#ifdef DAISY_PLAYER
      if (strcasecmp (misc->tag, "audio") == 0)
      {
         misc->prev_id = strdup (misc->audio_id);
         misc->audio_id = strdup (misc->current_id);
      } // if
#endif
      return 1;
   case XML_READER_TYPE_END_ELEMENT:
      snprintf (misc->tag, MAX_TAG - 1, "/%s",
                (char *) xmlTextReaderName (reader));
      return 1;
   case XML_READER_TYPE_TEXT:
   case XML_READER_TYPE_CDATA:
   case XML_READER_TYPE_SIGNIFICANT_WHITESPACE:
   {
      int x;
      size_t phrase_len;

      phrase_len = strlen ((char *) xmlTextReaderConstValue (reader));
      x = 0;
      while (1)
      {
         if (isspace (xmlTextReaderValue (reader)[x]))
            x++;
         else
            break;
      } // while
      misc->label = realloc (misc->label, phrase_len + 1);
      snprintf (misc->label, phrase_len - x + 1, "%s", (char *)
               xmlTextReaderValue (reader) + x);
      for (x = strlen (misc->label) - 1; x >= 0 && isspace (misc->label[x]);
           x--)
         misc->label[x] = 0;
      for (x = 0; misc->label[x] > 0; x++)
         if (! isprint (misc->label[x]))
            misc->label[x] = ' ';
      return 1;
   }
   case XML_READER_TYPE_ENTITY_REFERENCE:
      snprintf (misc->tag, MAX_TAG - 1, "/%s",
                (char *) xmlTextReaderName (reader));
      return 1;
   default:
      return 1;
   } // switch
   return 0;
} // get_tag_or_label

void go_to_page_number (misc_t *misc, my_attribute_t *my_attribute,
                          daisy_t *daisy)
{
   char pn[15];

   kill_player (misc);
#ifdef DAISY_PLAYER
   if (misc->cd_type != CDIO_DISC_MODE_CD_DA)
      misc->player_pid = -2;
#endif
   misc->playing = misc->just_this_item = -1;
   view_screen (misc, daisy);
#ifdef EBOOK_SPEAKER
   remove (misc->tmp_wav);
#endif             
   unlink ("old.wav");
   mvwprintw (misc->titlewin, 1, 0,
              "----------------------------------------");
   wprintw (misc->titlewin, "----------------------------------------");
   mvwprintw (misc->titlewin, 1, 0, "%s ", gettext ("Go to page number:"));
   echo ();
   wgetnstr (misc->titlewin, pn, 5);
   noecho ();
   view_screen (misc, daisy);
   if (atoi (pn) == 0 || atoi (pn) > misc->total_pages)
   {
      beep ();
      pause_resume (misc, my_attribute, daisy);
      pause_resume (misc, my_attribute, daisy);
      return;
   } // if

// start searching
   mvwprintw (misc->titlewin, 1, 0,
              "----------------------------------------");
   wprintw (misc->titlewin, "----------------------------------------");
   wattron (misc->titlewin, A_BOLD);
   mvwprintw (misc->titlewin, 1, 0, "%s", gettext ("Please wait..."));
   wattroff (misc->titlewin, A_BOLD);
   wrefresh (misc->titlewin);
   misc->playing = misc->current_page_number = 0;
#ifdef EBOOK_SPEAKER
   misc->phrase_nr = 0;
#endif

#ifdef DAISY_PLAYER
   open_clips_file (misc, my_attribute, daisy[misc->playing].clips_file,
                    daisy[misc->playing].clips_anchor);
#endif
#ifdef EBOOK_SPEAKER
   open_text_file (misc, my_attribute, daisy[misc->playing].xml_file,
                   daisy[misc->playing].anchor);
#endif
   while (1)
   {
      if (misc->current_page_number == atoi (pn))
      {
         misc->displaying = misc->current = misc->playing;
         misc->player_pid = -2;
         view_screen (misc, daisy);
         return;
      } // if (misc->current_page_number == atoi (pn))
      if (misc->current_page_number > atoi (pn))
      {
         misc->current = misc->playing = 0;
         misc->current_page_number = daisy[misc->playing].page_number;
#ifdef DAISY_PLAYER
         open_clips_file (misc, my_attribute, daisy[misc->playing].clips_file,
                          daisy[misc->playing].clips_anchor);
#endif
#ifdef EBOOK_SPEAKER
         open_text_file (misc, my_attribute, daisy[misc->playing].xml_file,
                         daisy[misc->playing].anchor);
#endif
         return;
      } // if
#ifdef DAISY_PLAYER
      get_next_clips (misc, my_attribute, daisy);
#endif
#ifdef EBOOK_SPEAKER
      get_next_phrase (misc, my_attribute, daisy, 0);
#endif
   } // while
} // go_to_page_number

void select_next_output_device (misc_t *misc, my_attribute_t *my_attribute,
                                daisy_t *daisy)
{
   FILE *r;
   int n, y, playing;
   size_t bytes;
   char *list[10], *trash;

   playing= misc->playing;
   pause_resume (misc, my_attribute, daisy);
   wclear (misc->screenwin);
   wprintw (misc->screenwin, "\n%s\n\n", gettext ("Select a soundcard:"));
   if (! (r = fopen ("/proc/asound/cards", "r")))
      failure (misc, gettext ("Cannot read /proc/asound/cards"), errno);
   for (n = 0; n < 10; n++)
   {
      list[n] = (char *) malloc (1000);
      bytes = getline (&list[n], &bytes, r);
      if ((int) bytes == -1)
         break;
      trash = (char *) malloc (1000);
      bytes = getline (&trash, &bytes, r);
      free (trash);
      wprintw (misc->screenwin, "   %s", list[n]);
      free (list[n]);
   } // for
   fclose (r);
   y = 3;
   nodelay (misc->screenwin, FALSE);
   for (;;)
   {
      wmove (misc->screenwin, y, 2);
      switch (wgetch (misc->screenwin))
      {
      case 13: //
         misc->sound_dev = malloc (3);
         snprintf (misc->sound_dev, 3, "%d", y - 3);
         view_screen (misc, daisy);
         nodelay (misc->screenwin, TRUE);
         if (playing > -1)
            pause_resume (misc, my_attribute, daisy);
         return;
      case KEY_DOWN:
         if (++y == n + 3)
            y = 3;
         break;
      case KEY_UP:
         if (--y == 2)
            y = n + 2;
         break;
      default:
         view_screen (misc, daisy);
         nodelay (misc->screenwin, TRUE);
         if (playing > -1)
            pause_resume (misc, my_attribute, daisy);
         return;
      } // switch
   } // for
} // select_next_output_device