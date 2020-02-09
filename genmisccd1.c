/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#define CD_OK (0)
#define CD_ERR_ARG (-1)
#define CD_ERR_FILE (-2)
#define CD_ERR_MEM (-3)

#define CD_WARN "WARN: "

typedef struct
{
  size_t m;
  size_t s;
  size_t f;
} trk_index_t;

typedef struct
{
  uint16_t l;
  uint16_t r;
} sample_16_t;

typedef struct
{
  uint8_t b;
  uint8_t a;
  uint8_t d;
  uint8_t c;
} sample_raw_t;

typedef union
{
  sample_16_t s;
  sample_raw_t r;
} sample_t;

static const int sample_size = 4;
static const int fd = 44100;
static const size_t frame_size = 588U;
static const size_t silence_size_A = 2250U;
static const size_t silence_strip_count_A = 5U;	// Odd number
static const char *performer = "Waveform generator";
static const size_t track_number_pulse = 3U;
static const size_t track_number_triangle = 5U;
static const size_t track_number_am = 3U;
static const size_t track_size_am = 18000U;	// 9000U;
static const size_t track_size_fm = 25000U;	//5000U;

trk_index_t calculate_index (const size_t offset);
int generate_image (const char *base_name);
int write_header (FILE * toc, FILE * cue);
int write_track_pulse (const int trk_i, const int trk_p, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname);
int write_track_square (const int trk_i, const int trk_p, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname);
int write_track_triangle (const int trk_i, const int trk_p, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname);
int write_track_am_sine (const int trk_i, const int trk_am, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname);
int write_track_am_triangle (const int trk_i, const int trk_am, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname);
int write_track_fm_step (const int trk_i, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname);
int write_noise (const int trk_i, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname);
int write_silence (const int trk_i, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname);

int
main (int argc, char **argv)
{
  int ret = CD_OK;

  if (2 == argc)
    {
      ret = generate_image (argv[1]);
    }
  else
    {
      fprintf (stderr, "Incorrect arg.\nUsage: %s outbasename\n\n", argv[0]);
      ret = CD_ERR_ARG;
    }

  return ret;
}

trk_index_t
calculate_index (const size_t offset_s)
{
  trk_index_t ret;

  const size_t offset = offset_s / frame_size;
  const size_t deviation = offset_s % frame_size;

  if (deviation)
    {
      fprintf (stderr, "Calculated index deviation %lld\n", (long long int) deviation);
    }

  const size_t div_m = 4500U;
  const size_t div_s = 75U;

  ret.m = offset / div_m;
  ret.s = (offset % div_m) / div_s;
  ret.f = (offset % div_m % div_s);

  return ret;
}

int
generate_image (const char *base_name)
{
  int ret = -10;
  size_t pos = 0;
  char *cdimg_name = malloc (strlen (base_name) + 4);
  char *toc_name = malloc (strlen (base_name) + 4);
  char *cue_name = malloc (strlen (base_name) + 4);
  strcpy (cdimg_name, base_name);
  strcpy (toc_name, base_name);
  strcpy (cue_name, base_name);
  strcat (cdimg_name, ".cdr");
  strcat (toc_name, ".toc");
  strcat (cue_name, ".cue");

  if ((NULL != cdimg_name) && (NULL != toc_name) && (NULL != cue_name))
    {
      FILE *cdimg = fopen (cdimg_name, "wb");
      FILE *toc = fopen (toc_name, "wt");
      FILE *cue = fopen (cue_name, "wt");

      if (cdimg && toc && cue)
	{
	  ret = write_header (toc, cue);

	  if (CD_OK == ret)
	    {
	      int trk_i = 1U;
	      // White noise
	      ret = write_noise (trk_i, &pos, cdimg, toc, cue, base_name);
	      trk_i++;
	      // Square pulses
	      if (CD_OK == ret)
		{
		  for (size_t trk_p = 1; trk_p <= track_number_pulse; trk_p++, trk_i++)
		    {
		      ret = write_track_square (trk_i, trk_p, &pos, cdimg, toc, cue, base_name);
		      if (CD_OK != ret)
			{
			  break;
			}
		    }
		}
	      // Single pulses
	      if (CD_OK == ret)
		{
		  for (size_t trk_p = 1; trk_p <= track_number_pulse; trk_p++, trk_i++)
		    {
		      ret = write_track_pulse (trk_i, trk_p, &pos, cdimg, toc, cue, base_name);
		      if (CD_OK != ret)
			{
			  break;
			}
		    }
		}
	      // Triangle pulses
	      if (CD_OK == ret)
		{
		  for (size_t trk_t = 1; trk_t <= track_number_triangle; trk_t++, trk_i++)
		    {
		      ret = write_track_triangle (trk_i, trk_t, &pos, cdimg, toc, cue, base_name);
		      if (CD_OK != ret)
			{
			  break;
			}
		    }
		}
	      // AM sine
	      if (CD_OK == ret)
		{
		  for (size_t trk_t = 1; trk_t <= track_number_am; trk_t++, trk_i++)
		    {
		      ret = write_track_am_sine (trk_i, trk_t, &pos, cdimg, toc, cue, base_name);
		      if (CD_OK != ret)
			{
			  break;
			}
		    }
		}
	      // AM triangle
	      if (CD_OK == ret)
		{
		  for (size_t trk_t = 1; trk_t <= track_number_am; trk_t++, trk_i++)
		    {
		      ret = write_track_am_triangle (trk_i, trk_t, &pos, cdimg, toc, cue, base_name);
		      if (CD_OK != ret)
			{
			  break;
			}
		    }
		}
	      // FM step
	      if (CD_OK == ret)
		{
		  ret = write_track_fm_step (trk_i, &pos, cdimg, toc, cue, base_name);
		}
	      if (CD_OK == ret)
		{
		  ret = write_silence (trk_i, &pos, cdimg, toc, cue, base_name);
		}
	    }
	  fclose (cdimg);
	  fclose (toc);
	  fclose (cue);
	}
      else
	{
	  fprintf (stderr, "Error opening files!\nTerminating!!!\n\n");
	  exit (1);
	}
    }
  else
    {
      fprintf (stderr, "Error allocating memory\n\n");
      ret = CD_ERR_MEM;
    }

  fprintf (stderr, "\nDone.\n\n");


  free (cdimg_name);
  free (toc_name);

  return ret;
}

int
write_header (FILE * toc, FILE * cue)
{
  int ret = CD_OK;
  const char *title = "Miscellaneous waveforms";
  const char *message = "All frequencies are fraction of FD to avoid beating";

  int pr_ret = fprintf (toc,
			"CD_DA\n"
			"\n"
			"CD_TEXT {\n"
			"  LANGUAGE_MAP {\n"
			"    0: 9\n"
                        "  }\n"
                        "  LANGUAGE 0 {\n"
                        "    TITLE \"%s\"\n"
                        "    PERFORMER \"%s\"\n"
                        "    MESSAGE \"%s\"\n"
                        "  }\n"
                        "}\n",
			title,
			performer,
			message);

  if (0 > pr_ret)
    {
      fprintf (stderr, "Write error (toc): %s!\n\n", strerror (errno));
      ret = CD_ERR_FILE;
    }

  pr_ret = fprintf (cue, "PERFORMER \"%s\"\n"
                         "TITLE \"%s\"\n"
                         "REM MESSAGE \"%s\"\n",
                    performer, title, message);

  if (0 > pr_ret)
    {
      fprintf (stderr, "Write error (cue): %s!\n\n", strerror (errno));
      ret = CD_ERR_FILE;
    }

  return ret;
}

int
write_track_pulse (const int trk_i, const int trk_p, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname)
{
  const size_t track_size_pulse = 4500U;
  int ret = CD_OK;
  double freq = 0.0;
  int div = 0;
  const size_t begin_pos = *pos;
  const int begin_frame = begin_pos / frame_size;
  const size_t end = begin_pos + (track_size_pulse * frame_size);
  const size_t track_length = end - begin_pos;

  const trk_index_t begin_pos_idx = calculate_index (begin_pos);
  const trk_index_t track_length_idx = calculate_index (track_length);

  fprintf (stderr, "===\nwrite_track: trk_i=%d, *pos=%lu\n", trk_i, *pos);

  // Write pulse data
  if (CD_OK == ret)
    {
      size_t buf_len = frame_size / 6U;
      for (size_t mi = trk_p; mi < track_number_pulse; mi++)
	{
	  buf_len *= 5U;
	}
      const size_t halflen = buf_len / 2U;
      const size_t bufsize = halflen * 2U * sample_size;
      fprintf (stderr, "Track %02d: buf_len:%lu halflen:%lu bufsize:%lu\n", trk_i, buf_len, halflen, bufsize);
      uint8_t *buf = malloc (bufsize);
      sample_t *sam = malloc (sizeof (sample_t) * buf_len);

      freq = (double) fd / (double) buf_len;
      div = (int) buf_len;

      if (buf && sam)
	{
	  memset (buf, 0, bufsize);
	  memset (sam, 0, sizeof (sample_t) * buf_len);

	  for (size_t i = 0U; i < halflen; i++)
	    {
	      int val1 = 0;
	      if (0U == i)
		{
		  val1 = 0X7FFF;
		}
	      int val2 = -val1 - 1;
	      if (-1 != (val1 + val2))
		{
		  fprintf (stderr, CD_WARN "Values are not symmetrical to neutral level 0.5: i=%d val1=%5d val2=%5d\n", (int) i, val1, val2);
		}

	      sam[i].s.l = (uint16_t) val1;
	      sam[i + halflen].s.l = (uint16_t) val2;
	      sam[i].s.r = (uint16_t) val1;
	      sam[i + halflen].s.r = (uint16_t) val2;
	    }

	  size_t buf_pos = 0U;

	  for (size_t i = 0; i < buf_len; i++)
	    {
	      // fprintf(stderr, "i=%lu sam:%02x:%02x:%02x:%02x\n", i, (int)sam[i].r.a, (int)sam[i].r.b, (int)sam[i].r.c, (int)sam[i].r.d);
	      buf[buf_pos++] = sam[i].r.a;
	      buf[buf_pos++] = sam[i].r.b;
	      buf[buf_pos++] = sam[i].r.c;
	      buf[buf_pos++] = sam[i].r.d;
	    }

	  while (end > *pos)
	    {
	      size_t chunks_wr = fwrite (buf, bufsize, 1, cdimg);
	      if (1 == chunks_wr)
		{
		  (*pos) += buf_len;
		}
	      else
		{
		  fprintf (stderr, "Write error (data): %s!\n\n", strerror (errno));
		  ret = CD_ERR_FILE;
		  break;
		}
	    }
	}
      else
	{
	  fprintf (stderr, "Memory allocation error(data): %s!\n\n", strerror (errno));
	  ret = CD_ERR_MEM;
	}

      free (buf);
      free (sam);
    }

  const size_t next_pos = *pos;
  const int next_frame = next_pos / frame_size;
  const int begin_dev = (begin_frame * frame_size) - (int) begin_pos;
  const int next_dev = (next_frame * frame_size) - (int) next_pos;

  fprintf (stderr, "Track %02d: Position: (c:%10lu | n:%10lu) Deviation: (c:%4d | n:%4d) Frames: (c:%7d | n:%7d)\n",
	   trk_i, begin_pos, next_pos, begin_dev, next_dev, begin_frame, next_frame);

  // Wtite TOC and CUE entry
  if (CD_OK == ret)
    {
      char title[200];
      char message[200];

      snprintf (title, sizeof (title), "Pop pulses %3.0f Hz", freq);
      snprintf (message, sizeof (message), "FD (%d Hz) divided by %2d%s", fd, div, (2.01 < ((double) fd / freq)) ? "" : " (Frequency outside filter range)");

      // TOC
      int pr_ret = fprintf (toc,
			    "\n"
			    "// Track %d\n"
			    "TRACK AUDIO\n"
			    "COPY\n"
			    "NO PRE_EMPHASIS\n"
			    "TWO_CHANNEL_AUDIO\n"
			    "CD_TEXT {\n"
			    "  LANGUAGE 0 {\n"
			    "    TITLE \"%s\"\n"
                            "    PERFORMER \"%s\"\n"
                            "    MESSAGE \"%s\"\n"
                            "  }\n"
                            "}\n"
                            "FILE \"%s.wav\" %02d:%02d:%02d %02d:%02d:%02d\n",
			    trk_i,
			    title,
			    performer,
			    message,
			    dataname,
			    (int) begin_pos_idx.m, (int) begin_pos_idx.s, (int) begin_pos_idx.f,
			    (int) track_length_idx.m, (int) track_length_idx.s, (int) track_length_idx.f);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (toc): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}

      // CUE
      char cue_indexes[200];
      cue_indexes[0] = 0;

      trk_index_t idx01 = calculate_index (begin_pos);

      snprintf (cue_indexes, sizeof (cue_indexes), "    INDEX 01 %02d:%02d:%02d\n", (int) idx01.m, (int) idx01.s, (int) idx01.f);

      pr_ret = fprintf (cue,
			"  TRACK %02d AUDIO\n"
			"    TITLE \"%s\"\n"
			"    PERFORMER \"%s\"\n"
                        "    REM MESSAGE \"%s\"\n"
                        "    FLAGS DCP\n"
                        "%s",
                        trk_i, title, performer, message, cue_indexes);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (cue): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}
    }

  return ret;
}

int
write_track_square (const int trk_i, const int trk_p, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname)
{
  const size_t track_size_pulse = 4500U;
  int ret = CD_OK;
  double freq = 0.0;
  int div = 0;
  const size_t begin_pos = *pos;
  const int begin_frame = begin_pos / frame_size;
  const size_t end = begin_pos + (track_size_pulse * frame_size);
  const size_t track_length = end - begin_pos;

  const trk_index_t begin_pos_idx = calculate_index (begin_pos);
  const trk_index_t track_length_idx = calculate_index (track_length);

  fprintf (stderr, "===\nwrite_track: trk_i=%d, *pos=%lu\n", trk_i, *pos);

  // Write square data
  if (CD_OK == ret)
    {
      size_t buf_len = frame_size / 6U;
      for (size_t mi = trk_p; mi < track_number_pulse; mi++)
	{
	  buf_len *= 5U;
	}
      const size_t halflen = buf_len / 2U;
      const size_t bufsize = halflen * 2U * sample_size;
      fprintf (stderr, "Track %02d: buf_len:%lu halflen:%lu bufsize:%lu\n", trk_i, buf_len, halflen, bufsize);
      uint8_t *buf = malloc (bufsize);
      sample_t *sam = malloc (sizeof (sample_t) * buf_len);

      freq = (double) fd / (double) buf_len;
      div = (int) buf_len;

      if (buf && sam)
	{
	  memset (buf, 0, bufsize);
	  memset (sam, 0, sizeof (sample_t) * buf_len);

	  for (size_t i = 0U; i < halflen; i++)
	    {
	      int val1 = 0X7FFF;
	      int val2 = -val1 - 1;
	      if (-1 != (val1 + val2))
		{
		  fprintf (stderr, CD_WARN "Values are not symmetrical to neutral level 0.5: i=%d val1=%5d val2=%5d\n", (int) i, val1, val2);
		}

	      sam[i].s.l = (uint16_t) val1;
	      sam[i + halflen].s.l = (uint16_t) val2;
	      sam[i].s.r = (uint16_t) val1;
	      sam[i + halflen].s.r = (uint16_t) val2;
	    }

	  size_t buf_pos = 0U;

	  for (size_t i = 0; i < buf_len; i++)
	    {
	      // fprintf(stderr, "i=%lu sam:%02x:%02x:%02x:%02x\n", i, (int)sam[i].r.a, (int)sam[i].r.b, (int)sam[i].r.c, (int)sam[i].r.d);
	      buf[buf_pos++] = sam[i].r.a;
	      buf[buf_pos++] = sam[i].r.b;
	      buf[buf_pos++] = sam[i].r.c;
	      buf[buf_pos++] = sam[i].r.d;
	    }

	  while (end > *pos)
	    {
	      size_t chunks_wr = fwrite (buf, bufsize, 1, cdimg);
	      if (1 == chunks_wr)
		{
		  (*pos) += buf_len;
		}
	      else
		{
		  fprintf (stderr, "Write error (data): %s!\n\n", strerror (errno));
		  ret = CD_ERR_FILE;
		  break;
		}
	    }
	}
      else
	{
	  fprintf (stderr, "Memory allocation error(data): %s!\n\n", strerror (errno));
	  ret = CD_ERR_MEM;
	}

      free (buf);
      free (sam);
    }

  const size_t next_pos = *pos;
  const int next_frame = next_pos / frame_size;
  const int begin_dev = (begin_frame * frame_size) - (int) begin_pos;
  const int next_dev = (next_frame * frame_size) - (int) next_pos;

  fprintf (stderr, "Track %02d: Position: (c:%10lu | n:%10lu) Deviation: (c:%4d | n:%4d) Frames: (c:%7d | n:%7d)\n",
	   trk_i, begin_pos, next_pos, begin_dev, next_dev, begin_frame, next_frame);

  // Wtite TOC and CUE entry
  if (CD_OK == ret)
    {
      char title[200];
      char message[200];

      snprintf (title, sizeof (title), "Square pulses %3.0f Hz", freq);
      snprintf (message, sizeof (message), "FD (%d Hz) divided by %2d%s", fd, div, (2.01 < ((double) fd / freq)) ? "" : " (Frequency outside filter range)");

      // TOC
      int pr_ret = fprintf (toc,
			    "\n"
			    "// Track %d\n"
			    "TRACK AUDIO\n"
			    "COPY\n"
			    "NO PRE_EMPHASIS\n"
			    "TWO_CHANNEL_AUDIO\n"
			    "CD_TEXT {\n"
			    "  LANGUAGE 0 {\n"
			    "    TITLE \"%s\"\n"
                            "    PERFORMER \"%s\"\n"
                            "    MESSAGE \"%s\"\n"
                            "  }\n"
                            "}\n"
                            "FILE \"%s.wav\" %02d:%02d:%02d %02d:%02d:%02d\n",
			    trk_i,
			    title,
			    performer,
			    message,
			    dataname,
			    (int) begin_pos_idx.m, (int) begin_pos_idx.s, (int) begin_pos_idx.f,
			    (int) track_length_idx.m, (int) track_length_idx.s, (int) track_length_idx.f);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (toc): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}

      // CUE
      char cue_indexes[200];
      cue_indexes[0] = 0;

      trk_index_t idx01 = calculate_index (begin_pos);

      snprintf (cue_indexes, sizeof (cue_indexes), "    INDEX 01 %02d:%02d:%02d\n", (int) idx01.m, (int) idx01.s, (int) idx01.f);

      pr_ret = fprintf (cue,
			"  TRACK %02d AUDIO\n"
			"    TITLE \"%s\"\n"
			"    PERFORMER \"%s\"\n"
                        "    REM MESSAGE \"%s\"\n"
                        "    FLAGS DCP\n"
                        "%s",
                        trk_i, title, performer, message, cue_indexes);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (cue): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}
    }

  return ret;
}

int
write_track_triangle (const int trk_i, const int trk_t, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname)
{
  const size_t track_size_triangle = 0x8000;
  int ret = CD_OK;
  double freq = 0.0;
  int div = 0;
  const size_t begin_pos = *pos;
  const int begin_frame = begin_pos / frame_size;
  const size_t end = begin_pos + (track_size_triangle * frame_size);
  const size_t track_length = end - begin_pos;

  const trk_index_t begin_pos_idx = calculate_index (begin_pos);
  const trk_index_t track_length_idx = calculate_index (track_length);

  fprintf (stderr, "===\nwrite_track: trk_i=%d, *pos=%lu\n", trk_i, *pos);

  // Write triangle data
  if (CD_OK == ret)
    {
      size_t buf_len = 1U << (19U - (trk_t * 2));
      size_t half_len = buf_len / 2U;
      const size_t bufsize = buf_len * sample_size;
      fprintf (stderr, "Track %02d: buf_len:%lu bufsize:%lu\n", trk_i, buf_len, bufsize);
      uint8_t *buf = malloc (bufsize);
      sample_t *sam = malloc (sizeof (sample_t) * buf_len);

      freq = (double) fd / (double) buf_len;
      div = (int) buf_len;

      if (buf && sam)
	{
	  memset (buf, 0, bufsize);
	  memset (sam, 0, sizeof (sample_t) * buf_len);

	  int val = -(0x8000);
#if 0
	  val += (0xFFFF >> (0x13 - (trk_t << 1)));
#else
	  val += (0x10000 >> (0x13 - (trk_t << 1)));
#endif
	  for (size_t i = 0U; i < half_len; i++)
	    {
	      sam[i].s.l = (uint16_t) val;
	      sam[i].s.r = (uint16_t) val;
	      val += (1 << ((trk_t - 1) << 1));
	    }
	  for (size_t i = half_len; i < buf_len; i++)
	    {
	      val -= (1 << ((trk_t - 1) << 1));
	      sam[i].s.l = (uint16_t) val;
	      sam[i].s.r = (uint16_t) val;
	    }

	  size_t buf_pos = 0U;

	  for (size_t i = 0; i < buf_len; i++)
	    {
	      // fprintf(stderr, "i=%lu sam:%02x:%02x:%02x:%02x\n", i, (int)sam[i].r.a, (int)sam[i].r.b, (int)sam[i].r.c, (int)sam[i].r.d);
	      buf[buf_pos++] = sam[i].r.a;
	      buf[buf_pos++] = sam[i].r.b;
	      buf[buf_pos++] = sam[i].r.c;
	      buf[buf_pos++] = sam[i].r.d;
	    }

	  while (end > *pos)
	    {
	      size_t chunks_wr = fwrite (buf, bufsize, 1, cdimg);
	      if (1 == chunks_wr)
		{
		  (*pos) += buf_len;
		}
	      else
		{
		  fprintf (stderr, "Write error (data): %s!\n\n", strerror (errno));
		  ret = CD_ERR_FILE;
		  break;
		}
	    }
	}
      else
	{
	  fprintf (stderr, "Memory allocation error(data): %s!\n\n", strerror (errno));
	  ret = CD_ERR_MEM;
	}

      free (buf);
      free (sam);
    }

  const size_t next_pos = *pos;
  const int next_frame = next_pos / frame_size;
  const int begin_dev = (begin_frame * frame_size) - (int) begin_pos;
  const int next_dev = (next_frame * frame_size) - (int) next_pos;

  fprintf (stderr, "Track %02d: Position: (c:%10lu | n:%10lu) Deviation: (c:%4d | n:%4d) Frames: (c:%7d | n:%7d)\n",
	   trk_i, begin_pos, next_pos, begin_dev, next_dev, begin_frame, next_frame);

  // Wtite TOC and CUE entry
  if (CD_OK == ret)
    {
      char title[200];
      char message[200];

      snprintf (title, sizeof (title), "Triangle pulses %18.15f Hz", freq);
      snprintf (message, sizeof (message), "FD (%d Hz) divided by %2d%s", fd, div, (2.01 < ((double) fd / freq)) ? "" : " (Frequency outside filter range)");

      // TOC
      int pr_ret = fprintf (toc,
			    "\n"
			    "// Track %d\n"
			    "TRACK AUDIO\n"
			    "COPY\n"
			    "NO PRE_EMPHASIS\n"
			    "TWO_CHANNEL_AUDIO\n"
			    "CD_TEXT {\n"
			    "  LANGUAGE 0 {\n"
			    "    TITLE \"%s\"\n"
                            "    PERFORMER \"%s\"\n"
                            "    MESSAGE \"%s\"\n"
                            "  }\n"
                            "}\n"
                            "FILE \"%s.wav\" %02d:%02d:%02d %02d:%02d:%02d\n",
			    trk_i,
			    title,
			    performer,
			    message,
			    dataname,
			    (int) begin_pos_idx.m, (int) begin_pos_idx.s, (int) begin_pos_idx.f,
			    (int) track_length_idx.m, (int) track_length_idx.s, (int) track_length_idx.f);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (toc): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}

      // CUE
      char cue_indexes[200];
      cue_indexes[0] = 0;

      trk_index_t idx01 = calculate_index (begin_pos);

      snprintf (cue_indexes, sizeof (cue_indexes), "    INDEX 01 %02d:%02d:%02d\n", (int) idx01.m, (int) idx01.s, (int) idx01.f);

      pr_ret = fprintf (cue,
			"  TRACK %02d AUDIO\n"
			"    TITLE \"%s\"\n"
			"    PERFORMER \"%s\"\n"
                        "    REM MESSAGE \"%s\"\n"
                        "    FLAGS DCP\n"
                        "%s",
                        trk_i, title, performer, message, cue_indexes);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (cue): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}
    }

  return ret;
}

int
write_track_am_sine (const int trk_i, const int trk_am, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname)
{
  int ret = CD_OK;
  double freq = 0.0;
  double freq_carr = 0.0;
  int div = 0;
  const size_t begin_pos = *pos;
  const int begin_frame = begin_pos / frame_size;
  const size_t end = begin_pos + (track_size_am * frame_size);
  const size_t track_length = end - begin_pos;

  const trk_index_t begin_pos_idx = calculate_index (begin_pos);
  const trk_index_t track_length_idx = calculate_index (track_length);

  fprintf (stderr, "===\nwrite_track: trk_i=%d, *pos=%lu\n", trk_i, *pos);

  // Write wave data
  if (CD_OK == ret)
    {
      const size_t buf_len = 8820;	// 8820 -> 15 frames -> 5Hz // (1 << (track_number_am - trk_am + 1));
      const size_t halflen = buf_len / 2U;
      const size_t bufsize = halflen * 2U * sample_size;
      fprintf (stderr, "Track %02d: buf_len:%lu halflen:%lu bufsize:%lu\n", trk_i, buf_len, halflen, bufsize);
      uint8_t *buf = malloc (bufsize);
      sample_t *sam = malloc (sizeof (sample_t) * buf_len);

      size_t carr_div = 30;
      for (int am_i = 1; trk_am > am_i; am_i++)
	{
	  carr_div *= 7;
	}
      const double carr_div_d = (double) carr_div;
      const int base_i = 0x8000;
      const double base_d = (double) base_i;

      freq = (double) fd / (double) buf_len;
      freq_carr = freq * carr_div_d;
      div = (int) buf_len;

      if (buf && sam)
	{
	  double radpos = M_PI / halflen / 2;

	  memset (buf, 0, bufsize);

	  for (size_t i = 0; i < buf_len; i++)
	    {
	      const double half_d = 0.5;
	      const double one_d = 1.0;
	      const double carr_dval = sin (radpos * carr_div_d);
	      const double carr_denv = (-cos (radpos) + one_d) * half_d;

	      double dval = carr_denv * carr_dval * (base_d - half_d);
	      int val1 = (int) (dval + base_d);
	      val1 -= base_i;

	      sam[i].s.l = (uint16_t) val1;
	      sam[i].s.r = (uint16_t) val1;

	      radpos += (M_PI / halflen);
	    }

	  for (size_t i = 0; i < buf_len; i++)
	    {
	      if (-1 != ((int) ((int16_t) sam[i].s.l) + (int) ((int16_t) sam[buf_len - i - 1].s.l)))
		{
		  fprintf (stderr, CD_WARN "Values are not symmetrical to neutral level 0.5: i=%d val1=%5d val2=%5d\n", (int) i, (int) ((int16_t) sam[i].s.l),
			   (int) ((int16_t) sam[buf_len - i - 1].s.l));
		}
	    }

	  size_t buf_pos = 0U;

	  for (size_t i = 0; i < buf_len; i++)
	    {
	      //fprintf(stderr, "i=%lu sam:%02x:%02x:%02x:%02x\n", i, (int)sam[i].r.a, (int)sam[i].r.b, (int)sam[i].r.c, (int)sam[i].r.d);
	      buf[buf_pos++] = sam[i].r.a;
	      buf[buf_pos++] = sam[i].r.b;
	      buf[buf_pos++] = sam[i].r.c;
	      buf[buf_pos++] = sam[i].r.d;
	    }

	  while (end > *pos)
	    {
	      size_t chunks_wr = fwrite (buf, bufsize, 1, cdimg);
	      if (1 == chunks_wr)
		{
		  (*pos) += buf_len;
		}
	      else
		{
		  fprintf (stderr, "Write error (data): %s!\n\n", strerror (errno));
		  ret = CD_ERR_FILE;
		  break;
		}
	    }
	}
      else
	{
	  fprintf (stderr, "Memory allocation error(data): %s!\n\n", strerror (errno));
	  ret = CD_ERR_MEM;
	}

      free (buf);
      free (sam);
    }

  const size_t next_pos = *pos;
  const int next_frame = next_pos / frame_size;
  const int begin_dev = (begin_frame * frame_size) - (int) begin_pos;
  const int next_dev = (next_frame * frame_size) - (int) next_pos;

  fprintf (stderr, "Track %02d: Position: (c:%10lu | n:%10lu) Deviation: (c:%4d | n:%4d) Frames: (c:%7d | n:%7d)\n",
	   trk_i, begin_pos, next_pos, begin_dev, next_dev, begin_frame, next_frame);

  // Wtite TOC and CUE entry
  if (CD_OK == ret)
    {
      char title[200];
      char message[200];

      snprintf (title, sizeof (title), "Carrier %4.0f Hz AM modulated with %1.0f Hz sine", freq_carr, freq);
      snprintf (message, sizeof (message), "FD (%d Hz) divided by %2d%s", fd, div, (2.01 < ((double) fd / freq)) ? "" : " (Frequency outside filter range)");

      // TOC
      int pr_ret = fprintf (toc,
			    "\n"
			    "// Track %d\n"
			    "TRACK AUDIO\n"
			    "COPY\n"
			    "NO PRE_EMPHASIS\n"
			    "TWO_CHANNEL_AUDIO\n"
			    "CD_TEXT {\n"
			    "  LANGUAGE 0 {\n"
			    "    TITLE \"%s\"\n"
                            "    PERFORMER \"%s\"\n"
                            "    MESSAGE \"%s\"\n"
                            "  }\n"
                            "}\n"
                            "FILE \"%s.wav\" %02d:%02d:%02d %02d:%02d:%02d\n",
			    trk_i,
			    title,
			    performer,
			    message,
			    dataname,
			    (int) begin_pos_idx.m, (int) begin_pos_idx.s, (int) begin_pos_idx.f,
			    (int) track_length_idx.m, (int) track_length_idx.s, (int) track_length_idx.f);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (toc): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}

      // CUE
      char cue_indexes[200];
      cue_indexes[0] = 0;


      trk_index_t idx01 = calculate_index (begin_pos);
      snprintf (cue_indexes, sizeof (cue_indexes), "    INDEX 01 %02d:%02d:%02d\n", (int) idx01.m, (int) idx01.s, (int) idx01.f);

      pr_ret = fprintf (cue,
			"  TRACK %02d AUDIO\n"
			"    TITLE \"%s\"\n"
			"    PERFORMER \"%s\"\n"
                        "    REM MESSAGE \"%s\"\n"
                        "    FLAGS DCP\n"
                        "%s",
                        trk_i, title, performer, message, cue_indexes);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (cue): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}
    }

  return ret;
}

int
write_track_am_triangle (const int trk_i, const int trk_am, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname)
{
  int ret = CD_OK;
  double freq = 0.0;
  double freq_carr = 0.0;
  int div = 0;
  const size_t begin_pos = *pos;
  const int begin_frame = begin_pos / frame_size;
  const size_t end = begin_pos + (track_size_am * frame_size);
  const size_t track_length = end - begin_pos;

  const trk_index_t begin_pos_idx = calculate_index (begin_pos);
  const trk_index_t track_length_idx = calculate_index (track_length);

  fprintf (stderr, "===\nwrite_track: trk_i=%d, *pos=%lu\n", trk_i, *pos);

  // Write wave data
  if (CD_OK == ret)
    {
      const size_t buf_len = 8820;	// 8820 -> 15 frames -> 5Hz
      const size_t halflen = buf_len / 2U;
      const size_t bufsize = halflen * 2U * sample_size;
      fprintf (stderr, "Track %02d: buf_len:%lu halflen:%lu bufsize:%lu\n", trk_i, buf_len, halflen, bufsize);
      uint8_t *buf = malloc (bufsize);
      sample_t *sam = malloc (sizeof (sample_t) * buf_len);

      size_t carr_div = 30;
      for (int am_i = 1; trk_am > am_i; am_i++)
	{
	  carr_div *= 7;
	}
      const double carr_div_d = (double) carr_div;
      const int base_i = 0x8000;
      const double base_d = (double) base_i;

      freq = (double) fd / (double) buf_len;
      freq_carr = freq * carr_div_d;
      div = (int) buf_len;

      if (buf && sam)
	{
	  double radpos = M_PI / halflen / 2;

	  memset (buf, 0, bufsize);

	  for (size_t i = 0; i < buf_len; i++)
	    {
	      const double half_d = 0.5;
	      const double carr_dval = sin (radpos * carr_div_d);
	      double carr_denv = 0.0;
	      if (halflen > i)
		{
		  carr_denv = ((double) i + half_d) / (double) halflen;
		}
	      else
		{
		  carr_denv = ((double) (buf_len - i - 1) + half_d) / (double) halflen;
		}

	      double dval = carr_denv * carr_dval * (base_d - half_d);
	      int val1 = (int) (dval + base_d);
	      val1 -= base_i;

	      sam[i].s.l = (uint16_t) val1;
	      sam[i].s.r = (uint16_t) val1;

	      radpos += (M_PI / halflen);
	    }

	  for (size_t i = 0; i < buf_len; i++)
	    {
	      if (-1 != ((int) ((int16_t) sam[i].s.l) + (int) ((int16_t) sam[buf_len - i - 1].s.l)))
		{
		  fprintf (stderr, CD_WARN "Values are not symmetrical to neutral level 0.5: i=%d val1=%5d val2=%5d\n", (int) i, (int) ((int16_t) sam[i].s.l),
			   (int) ((int16_t) sam[buf_len - i - 1].s.l));
		}
	    }

	  size_t buf_pos = 0U;

	  for (size_t i = 0; i < buf_len; i++)
	    {
	      //fprintf(stderr, "i=%lu sam:%02x:%02x:%02x:%02x\n", i, (int)sam[i].r.a, (int)sam[i].r.b, (int)sam[i].r.c, (int)sam[i].r.d);
	      buf[buf_pos++] = sam[i].r.a;
	      buf[buf_pos++] = sam[i].r.b;
	      buf[buf_pos++] = sam[i].r.c;
	      buf[buf_pos++] = sam[i].r.d;
	    }

	  while (end > *pos)
	    {
	      size_t chunks_wr = fwrite (buf, bufsize, 1, cdimg);
	      if (1 == chunks_wr)
		{
		  (*pos) += buf_len;
		}
	      else
		{
		  fprintf (stderr, "Write error (data): %s!\n\n", strerror (errno));
		  ret = CD_ERR_FILE;
		  break;
		}
	    }
	}
      else
	{
	  fprintf (stderr, "Memory allocation error(data): %s!\n\n", strerror (errno));
	  ret = CD_ERR_MEM;
	}

      free (buf);
      free (sam);
    }

  const size_t next_pos = *pos;
  const int next_frame = next_pos / frame_size;
  const int begin_dev = (begin_frame * frame_size) - (int) begin_pos;
  const int next_dev = (next_frame * frame_size) - (int) next_pos;

  fprintf (stderr, "Track %02d: Position: (c:%10lu | n:%10lu) Deviation: (c:%4d | n:%4d) Frames: (c:%7d | n:%7d)\n",
	   trk_i, begin_pos, next_pos, begin_dev, next_dev, begin_frame, next_frame);

  // Wtite TOC and CUE entry
  if (CD_OK == ret)
    {
      char title[200];
      char message[200];

      snprintf (title, sizeof (title), "Carrier %4.0f Hz AM modulated with %1.0f Hz triangle", freq_carr, freq);
      snprintf (message, sizeof (message), "FD (%d Hz) divided by %2d%s", fd, div, (2.01 < ((double) fd / freq)) ? "" : " (Frequency outside filter range)");

      // TOC
      int pr_ret = fprintf (toc,
			    "\n"
			    "// Track %d\n"
			    "TRACK AUDIO\n"
			    "COPY\n"
			    "NO PRE_EMPHASIS\n"
			    "TWO_CHANNEL_AUDIO\n"
			    "CD_TEXT {\n"
			    "  LANGUAGE 0 {\n"
			    "    TITLE \"%s\"\n"
                            "    PERFORMER \"%s\"\n"
                            "    MESSAGE \"%s\"\n"
                            "  }\n"
                            "}\n"
                            "FILE \"%s.wav\" %02d:%02d:%02d %02d:%02d:%02d\n",
			    trk_i,
			    title,
			    performer,
			    message,
			    dataname,
			    (int) begin_pos_idx.m, (int) begin_pos_idx.s, (int) begin_pos_idx.f,
			    (int) track_length_idx.m, (int) track_length_idx.s, (int) track_length_idx.f);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (toc): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}

      // CUE
      char cue_indexes[200];
      cue_indexes[0] = 0;


      trk_index_t idx01 = calculate_index (begin_pos);
      snprintf (cue_indexes, sizeof (cue_indexes), "    INDEX 01 %02d:%02d:%02d\n", (int) idx01.m, (int) idx01.s, (int) idx01.f);

      pr_ret = fprintf (cue,
			"  TRACK %02d AUDIO\n"
			"    TITLE \"%s\"\n"
			"    PERFORMER \"%s\"\n"
                        "    REM MESSAGE \"%s\"\n"
                        "    FLAGS DCP\n"
                        "%s",
                        trk_i, title, performer, message, cue_indexes);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (cue): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}
    }

  return ret;
}

int
write_track_fm_step (const int trk_i, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname)
{
  int ret = CD_OK;
  const size_t begin_pos = *pos;
  const int begin_frame = begin_pos / frame_size;
  const size_t end = begin_pos + (track_size_fm * frame_size);
  const size_t track_length = end - begin_pos;
  const size_t buf_len = 2500U;
  const size_t step_num = 5U;
  const size_t step_ratio = 5U;
  const size_t buf_num = (step_num - 1U) * 2U;
  double freq_vals[step_num];
  double freq_envelope = 0.0;

  const trk_index_t begin_pos_idx = calculate_index (begin_pos);
  const trk_index_t track_length_idx = calculate_index (track_length);

  memset (freq_vals, 0, sizeof (freq_vals));

  fprintf (stderr, "===\nwrite_track: trk_i=%d, *pos=%lu\n", trk_i, *pos);

  // Write wave data
  if (CD_OK == ret)
    {
      size_t buf_ratio[buf_num];
      const size_t bufsize = buf_len * sample_size * buf_num;
      fprintf (stderr, "Track %02d: buf_len:%lu bufsize:%lu buf_num:%lu\n", trk_i, buf_len, bufsize, buf_num);
      uint8_t *buf = malloc (bufsize);
      sample_t *sam = malloc (sizeof (sample_t) * buf_len * buf_num);

      if (buf && sam)
	{
	  size_t i = 0;

	  memset (buf, 0, bufsize);
	  memset (sam, 0, sizeof (sample_t) * buf_len * buf_num);

	  size_t ri = 0U;
	  size_t r = 1U;
	  for (; step_num > ri; ri++)
	    {
	      freq_vals[ri] = (double) fd / (double) buf_len *(double) r;
	      buf_ratio[ri] = r;
	      r *= step_ratio;
	    }
	  r /= step_ratio;
	  for (; buf_num > ri; ri++)
	    {
	      r /= step_ratio;
	      buf_ratio[ri] = r;
	    }
	  freq_envelope = (double) fd / (double) buf_len / (double) buf_num;



	  for (size_t buf_i = 0U; buf_num > buf_i; buf_i++)
	    {
	      size_t br = buf_ratio[buf_i];

	      for (size_t peri = 0; br > peri; peri++)
		{
		  size_t halflen = buf_len / br / 2U;
		  double radpos = M_PI / (double) halflen / 2.0;


		  // fprintf(stderr, CD_WARN "buf_i=%lu br=%lu freq=%f radpos=%f halflen=%lu peri=%lu\n", buf_i, br, freq, radpos, halflen, peri);

		  for (size_t wave_i = 0; wave_i < halflen; i++, wave_i++)
		    {
		      const int base_i = 0x8000;
		      const double base_d = (double) base_i;
		      const double half_d = 0.5;
		      double dval = sin (radpos) * (base_d - half_d);
		      double dval_neg = -dval;
		      int val1 = (int) (dval + base_d);
		      int val2 = (int) (dval_neg + base_d);
		      val1 -= base_i;
		      val2 -= base_i;
		      if (-1 != (val1 + val2))
			{
			  fprintf (stderr, CD_WARN "Values are not symmetrical to neutral level 0.5: i=%d val1=%5d val2=%5d\n", (int) i, val1, val2);
			}

		      sam[i].s.l = (uint16_t) val1;
		      sam[i + halflen].s.l = (uint16_t) val2;
		      sam[i].s.r = (uint16_t) val1;
		      sam[i + halflen].s.r = (uint16_t) val2;

		      radpos += (M_PI / halflen);
		    }

		  i += halflen;

		}

	    }



	  size_t buf_pos = 0U;

	  for (size_t i = 0; i < buf_len * buf_num; i++)
	    {
	      // fprintf(stderr, "i=%lu sam:%02x:%02x:%02x:%02x\n", i, (int)sam[i].r.a, (int)sam[i].r.b, (int)sam[i].r.c, (int)sam[i].r.d);
	      buf[buf_pos++] = sam[i].r.a;
	      buf[buf_pos++] = sam[i].r.b;
	      buf[buf_pos++] = sam[i].r.c;
	      buf[buf_pos++] = sam[i].r.d;
	    }

	  while (end > *pos)
	    {
	      size_t chunks_wr = fwrite (buf, bufsize, 1, cdimg);
	      if (1 == chunks_wr)
		{
		  (*pos) += buf_len;
		}
	      else
		{
		  fprintf (stderr, "Write error (data): %s!\n\n", strerror (errno));
		  ret = CD_ERR_FILE;
		  break;
		}
	    }
	}
      else
	{
	  fprintf (stderr, "Memory allocation error(data): %s!\n\n", strerror (errno));
	  ret = CD_ERR_MEM;
	}

      free (buf);
      free (sam);
    }

  const size_t next_pos = *pos;
  const int next_frame = next_pos / frame_size;
  const int begin_dev = (begin_frame * frame_size) - (int) begin_pos;
  const int next_dev = (next_frame * frame_size) - (int) next_pos;

  fprintf (stderr, "Track %02d: Position: (c:%10lu | n:%10lu) Deviation: (c:%4d | n:%4d) Frames: (c:%7d | n:%7d)\n",
	   trk_i, begin_pos, next_pos, begin_dev, next_dev, begin_frame, next_frame);

  // Wtite TOC and CUE entry
  if (CD_OK == ret)
    {
      char title[200];
      char message[200];

      // double freq_vals[step_num];
      snprintf (title, sizeof (title), "Repeating frequencies %.3f, %.3f, %.3f, %.3f, %.3f Hz (0 dB) Envelope %5.3f Hz",
		freq_vals[0], freq_vals[1], freq_vals[2], freq_vals[3], freq_vals[4], freq_envelope);
      snprintf (message, sizeof (message), "FD (%d Hz) divided by %lu", fd, buf_len * buf_num);

      // TOC
      int pr_ret = fprintf (toc,
			    "\n"
			    "// Track %d\n"
			    "TRACK AUDIO\n"
			    "COPY\n"
			    "NO PRE_EMPHASIS\n"
			    "TWO_CHANNEL_AUDIO\n"
			    "CD_TEXT {\n"
			    "  LANGUAGE 0 {\n"
			    "    TITLE \"%s\"\n"
                            "    PERFORMER \"%s\"\n"
                            "    MESSAGE \"%s\"\n"
                            "  }\n"
                            "}\n"
                            "FILE \"%s.wav\" %02d:%02d:%02d %02d:%02d:%02d\n",
			    trk_i,
			    title,
			    performer,
			    message,
			    dataname,
			    (int) begin_pos_idx.m, (int) begin_pos_idx.s, (int) begin_pos_idx.f,
			    (int) track_length_idx.m, (int) track_length_idx.s, (int) track_length_idx.f);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (toc): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}

      // CUE
      char cue_indexes[200];
      cue_indexes[0] = 0;

      trk_index_t idx01 = calculate_index (begin_pos);

      snprintf (cue_indexes, sizeof (cue_indexes), "    INDEX 01 %02d:%02d:%02d\n", (int) idx01.m, (int) idx01.s, (int) idx01.f);

      pr_ret = fprintf (cue,
			"  TRACK %02d AUDIO\n"
			"    TITLE \"%s\"\n"
			"    PERFORMER \"%s\"\n"
                        "    REM MESSAGE \"%s\"\n"
                        "    FLAGS DCP\n"
                        "%s",
                        trk_i, title, performer, message, cue_indexes);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (cue): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}
    }

  return ret;
}

int
write_noise (const int trk_i, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname)
{
  const size_t pregap = 2940U;
  const size_t track_size_noise = 22500U;
  int ret = CD_OK;
  const size_t begin_pregap = *pos;
  const size_t begin_pos = *pos + pregap;
  const int begin_frame = begin_pos / frame_size;
  const size_t end = begin_pos + (track_size_noise * frame_size);
  const size_t track_length = end - begin_pregap;

  const trk_index_t begin_pos_idx = calculate_index (begin_pregap);
  const trk_index_t track_length_idx = calculate_index (track_length);

  fprintf (stderr, "===\nwrite_track: trk_i=%d, pregap=%lu, *pos=%lu\n", trk_i, pregap, *pos);

  // Write cue wavefile
  if (1)
    {
      int pr_ret = fprintf (cue,
			    "FILE \"%s.wav\" WAVE\n",
			    dataname);

      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (cue): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}
    }

  // Write a pregap
  if ((CD_OK == ret) && (0 < pregap))
    {
      const size_t sample_size = 4;
      char *pregap_buf = malloc (sample_size);
      if (NULL != pregap_buf)
	{
	  memset (pregap_buf, 0, sample_size);
	  for (size_t i = 0U; i < pregap; i++)
	    {
	      size_t chunks_wr = fwrite (pregap_buf, sample_size, 1, cdimg);
	      if (1 == chunks_wr)
		{
		  (*pos)++;
		}
	      else
		{
		  fprintf (stderr, "Write error (gap): %s!\n\n", strerror (errno));
		  ret = CD_ERR_FILE;
		  break;
		}
	    }
	}
      else
	{
	  fprintf (stderr, "Memory allocation error(gap): %s!\n\n", strerror (errno));
	  ret = CD_ERR_MEM;
	}
      free (pregap_buf);
    }

  // Write random data
  if (CD_OK == ret)
    {
      size_t buf_len = 588U;	// (1 << (tracks_num - trk_i + 1));
      size_t halflen = buf_len / 2U;
      size_t bufsize = halflen * 2U * sample_size;
      fprintf (stderr, "Track %02d: buf_len:%lu halflen:%lu bufsize:%lu\n", trk_i, buf_len, halflen, bufsize);
      uint8_t *buf = malloc (bufsize);
      sample_t *sam = malloc (sizeof (sample_t) * buf_len);

      srandom (0xb1e27b68);

      if (buf && sam)
	{
	  /*
	     long long int dc1 = 0;
	     long long int dc2 = 0;
	   */
	  memset (buf, 0, bufsize);

	  while (end > *pos)
	    {
	      for (size_t i = 0; i < buf_len; i++)
		{
		  int val1 = (int) (0xFFFF & random ()) - 0x8000;
		  int val2 = (int) (0xFFFF & random ()) - 0x8000;

		  /*
		     dc1 += val1;
		     dc2 += val2;
		   */

		  sam[i].s.l = (uint16_t) val1;
		  sam[i].s.r = (uint16_t) val2;

		  /* fprintf(stderr, "DC %+12lld %+12lld\r", dc1, dc2); */
/*
          fprintf(stderr, "VAL1 %+06d\n", val1);
          fprintf(stderr, "VAL2 %+06d\n", val2);
*/
		}

	      size_t buf_pos = 0U;

	      for (size_t i = 0; i < buf_len; i++)
		{
		  // fprintf(stderr, "i=%lu sam:%02x:%02x:%02x:%02x\n", i, (int)sam[i].r.a, (int)sam[i].r.b, (int)sam[i].r.c, (int)sam[i].r.d);
		  buf[buf_pos++] = sam[i].r.a;
		  buf[buf_pos++] = sam[i].r.b;
		  buf[buf_pos++] = sam[i].r.c;
		  buf[buf_pos++] = sam[i].r.d;
		}

	      size_t chunks_wr = fwrite (buf, bufsize, 1, cdimg);
	      if (1 == chunks_wr)
		{
		  (*pos) += buf_len;
		}
	      else
		{
		  fprintf (stderr, "Write error (data): %s!\n\n", strerror (errno));
		  ret = CD_ERR_FILE;
		  break;
		}
	    }
	  /*fprintf(stderr, "\nDC done\n"); */
	}
      else
	{
	  fprintf (stderr, "Memory allocation error(data): %s!\n\n", strerror (errno));
	  ret = CD_ERR_MEM;
	}

      free (buf);
      free (sam);
    }

  const size_t next_pos = *pos;
  const int next_frame = next_pos / frame_size;
  const int begin_dev = (begin_frame * frame_size) - (int) begin_pos;
  const int next_dev = (next_frame * frame_size) - (int) next_pos;

  fprintf (stderr, "Track %02d: Position: (c:%10lu | n:%10lu) Deviation: (c:%4d | n:%4d) Frames: (c:%7d | n:%7d)\n",
	   trk_i, begin_pos, next_pos, begin_dev, next_dev, begin_frame, next_frame);

  // Wtite TOC and CUE entry
  if (CD_OK == ret)
    {
      const char *title = "White noise";
      const char *message = "Random values";
      char pregap_line[80];

      trk_index_t pre = calculate_index (pregap);
      snprintf (pregap_line, sizeof (pregap_line), "START %02d:%02d:%02d\n", (int) pre.m, (int) pre.s, (int) pre.f);

      // TOC
      int pr_ret = fprintf (toc,
			    "\n"
			    "// Track %d\n"
			    "TRACK AUDIO\n"
			    "COPY\n"
			    "NO PRE_EMPHASIS\n"
			    "TWO_CHANNEL_AUDIO\n"
			    "CD_TEXT {\n"
			    "  LANGUAGE 0 {\n"
			    "    TITLE \"%s\"\n"
			    "    PERFORMER \"%s\"\n"
                            "    MESSAGE \"%s\"\n"
                            "  }\n"
                            "}\n"
                            "FILE \"%s.wav\" %02d:%02d:%02d %02d:%02d:%02d\n"
                            "%s\n",
			    trk_i,
			    title,
			    performer,
			    message,
			    dataname,
			    (int) begin_pos_idx.m, (int) begin_pos_idx.s, (int) begin_pos_idx.f,
			    (int) track_length_idx.m, (int) track_length_idx.s, (int) track_length_idx.f,
			    pregap ? pregap_line : "");
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (toc): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}

      // CUE
      char cue_indexes[200];
      cue_indexes[0] = 0;

      trk_index_t idx00 = calculate_index (begin_pregap);
      trk_index_t idx01 = calculate_index (begin_pos);

      if (pregap)
	{
	  snprintf (cue_indexes, sizeof (cue_indexes), "    INDEX 00 %02d:%02d:%02d\n    INDEX 01 %02d:%02d:%02d\n",
		    (int) idx00.m, (int) idx00.s, (int) idx00.f, (int) idx01.m, (int) idx01.s, (int) idx01.f);
	}
      else
	{
	  snprintf (cue_indexes, sizeof (cue_indexes), "    INDEX 01 %02d:%02d:%02d\n", (int) idx01.m, (int) idx01.s, (int) idx01.f);
	}

      pr_ret = fprintf (cue,
			"  TRACK %02d AUDIO\n"
			"    TITLE \"%s\"\n"
			"    PERFORMER \"%s\"\n"
                        "    REM MESSAGE \"%s\"\n"
                        "    FLAGS DCP\n"
                        "%s",
                        trk_i, title, performer, message, cue_indexes);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (cue): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}
    }

  return ret;
}

int
write_silence (const int trk_i, size_t *pos, FILE * cdimg, FILE * toc, FILE * cue, const char *dataname)
{
  int ret = CD_OK;
  const size_t begin_pos = *pos;
  const int begin_frame = begin_pos / frame_size;
  const size_t end = begin_pos + (silence_size_A * silence_strip_count_A * frame_size);
  const size_t track_length = end - begin_pos;
  const size_t index_entries_initial_size = 0x100U;

  const trk_index_t begin_pos_idx = calculate_index (begin_pos);
  const trk_index_t track_length_idx = calculate_index (track_length);

  fprintf (stderr, "===\nwrite_silence: trk_i=%d, *pos=%lu\n", trk_i, *pos);

  char *index_entries = malloc (index_entries_initial_size);
  if (index_entries)
    {
      memset (index_entries, 0, index_entries_initial_size);
    }
  else
    {
      fprintf (stderr, "Memory allocation error(silence): %s!\n\n", strerror (errno));
      ret = CD_ERR_MEM;
    }

  char *index_cue = malloc (index_entries_initial_size);
  if (index_cue)
    {
      memset (index_cue, 0, index_entries_initial_size);
    }
  else
    {
      fprintf (stderr, "Memory allocation error(silence cue): %s!\n\n", strerror (errno));
      ret = CD_ERR_MEM;
    }

  // Write wave data
  if (CD_OK == ret)
    {
      int cue_idx_i = 2;
      size_t buf_len = 2U;
      const size_t chunk_cnt = frame_size / buf_len;
      size_t bufsize = buf_len * sample_size;
      fprintf (stderr, "Track %02d: buf_len:%lu bufsize:%lu\n", trk_i, buf_len, bufsize);
      uint8_t *buf = malloc (bufsize);
      sample_t *sam = malloc (sizeof (sample_t) * buf_len);

      if (buf && sam && index_entries && index_cue)
	{
	  memset (buf, 0, bufsize);

	  for (size_t si = 0; si < silence_strip_count_A; si++)
	    {
	      const size_t index_pos = *pos - begin_pos;
	      trk_index_t idx = calculate_index (index_pos);
	      if (0U < index_pos)
		{
		  char index[100];

		  // TOC
		  snprintf (index, sizeof (index), "INDEX %02d:%02d:%02d\n", (int) (idx.m), (int) (idx.s), (int) (idx.f));
		  index_entries = realloc (index_entries, strlen (index_entries) + strlen (index) + 1U);
		  if (index_entries)
		    {
		      strcat (index_entries, index);
		    }
		  else
		    {
		      fprintf (stderr, "Memory re-allocation error(silence): %s!\n\n", strerror (errno));
		      ret = CD_ERR_MEM;
		    }

		  // CUE
		  trk_index_t idx_cue = calculate_index (*pos);
		  snprintf (index, sizeof (index), "    INDEX %02d %02d:%02d:%02d\n", cue_idx_i, (int) (idx_cue.m), (int) (idx_cue.s), (int) (idx_cue.f));
		  index_cue = realloc (index_cue, strlen (index_cue) + strlen (index) + 1U);
		  if (index_cue)
		    {
		      strcat (index_cue, index);
		    }
		  else
		    {
		      fprintf (stderr, "Memory re-allocation error(silence): %s!\n\n", strerror (errno));
		      ret = CD_ERR_MEM;
		    }
		  cue_idx_i++;
		}


	      sam[0].s.l = 0U;
	      sam[1].s.l = (si % 2U) ? ((uint16_t) (-1)) : 0U;
	      sam[0].s.r = sam[0].s.l;
	      sam[1].s.r = sam[1].s.l;

	      size_t buf_pos = 0U;

	      for (size_t ci = 0; ci < buf_len; ci++)
		{
		  // fprintf(stderr, "ci=%lu sam:%02x:%02x:%02x:%02x\n", ci, (int)sam[ci].r.a, (int)sam[ci].r.b, (int)sam[ci].r.c, (int)sam[ci].r.d);
		  buf[buf_pos++] = sam[ci].r.a;
		  buf[buf_pos++] = sam[ci].r.b;
		  buf[buf_pos++] = sam[ci].r.c;
		  buf[buf_pos++] = sam[ci].r.d;
		}

	      for (size_t i = 0; i < silence_size_A; i++)
		{
		  for (size_t i = 0; i < chunk_cnt; i++)
		    {
		      size_t chunks_wr = fwrite (buf, bufsize, 1, cdimg);
		      if (1 == chunks_wr)
			{
			  (*pos) += buf_len;
			}
		      else
			{
			  fprintf (stderr, "Write error (silence): %s!\n\n", strerror (errno));
			  ret = CD_ERR_FILE;
			  break;
			}
		    }
		}
	    }
	}
      else
	{
	  fprintf (stderr, "Memory allocation error(silence): %s!\n\n", strerror (errno));
	  ret = CD_ERR_MEM;
	}

      free (buf);
      free (sam);
    }

  const size_t next_pos = *pos;
  const int next_frame = next_pos / frame_size;
  const int begin_dev = (begin_frame * frame_size) - (int) begin_pos;
  const int next_dev = (next_frame * frame_size) - (int) next_pos;

  fprintf (stderr, "Track %02d: Position: (c:%10lu | n:%10lu) Deviation: (c:%4d | n:%4d) Frames: (c:%7d | n:%7d)\n",
	   trk_i, begin_pos, next_pos, begin_dev, next_dev, begin_frame, next_frame);

  // Wtite TOC and CUE entry
  if (CD_OK == ret)
    {
      char title[200];
      char message[200];

      double strip_duration = ((double) silence_size_A * (double) frame_size) / (double) fd;

      snprintf (title, sizeof (title), "Silence");
      snprintf (message, sizeof (message), "Repeating %1.1f seconds constant zero level and %1.1f seconds clapping silence", strip_duration, strip_duration);

      // TOC
      int pr_ret = fprintf (toc,
			    "\n"
			    "// Track %d\n"
			    "TRACK AUDIO\n"
			    "COPY\n"
			    "NO PRE_EMPHASIS\n"
			    "TWO_CHANNEL_AUDIO\n"
			    "CD_TEXT {\n"
			    "  LANGUAGE 0 {\n"
			    "    TITLE \"%s\"\n"
			    "    PERFORMER \"%s\"\n"
                            "    MESSAGE \"%s\"\n"
                            "  }\n"
                            "}\n"
                            "FILE \"%s.wav\" %02d:%02d:%02d %02d:%02d:%02d\n"
                            "%s\n",
			    trk_i,
			    title,
			    performer,
			    message,
			    dataname,
			    (int) begin_pos_idx.m, (int) begin_pos_idx.s, (int) begin_pos_idx.f,
			    (int) track_length_idx.m, (int) track_length_idx.s, (int) track_length_idx.f,
			    index_entries);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (toc): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}

      // CUE
      char cue_indexes[200];
      cue_indexes[0] = 0;

      trk_index_t idx01 = calculate_index (begin_pos);

      {
	snprintf (cue_indexes, sizeof (cue_indexes), "    INDEX 01 %02d:%02d:%02d\n", (int) idx01.m, (int) idx01.s, (int) idx01.f);
      }

      pr_ret = fprintf (cue,
			"  TRACK %02d AUDIO\n"
			"    TITLE \"%s\"\n"
			"    PERFORMER \"%s\"\n"
                        "    REM MESSAGE \"%s\"\n"
                        "    FLAGS DCP\n"
                        "%s%s",
                        trk_i, title, performer, message, cue_indexes, index_cue);
      if (0 > pr_ret)
	{
	  fprintf (stderr, "Write error (cue): %s!\n\n", strerror (errno));
	  ret = CD_ERR_FILE;
	}
    }

  free (index_entries);
  free (index_cue);

  return ret;
}
