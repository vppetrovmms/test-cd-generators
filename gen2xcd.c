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

typedef struct {
  size_t m;
  size_t s;
  size_t f;
} trk_index_t;

typedef struct {
  uint16_t l;
  uint16_t r;
} sample_16_t;

typedef struct {
  uint8_t b;
  uint8_t a;
  uint8_t d;
  uint8_t c;
} sample_raw_t;

typedef union {
  sample_16_t s;
  sample_raw_t r;
} sample_t;

static const int sample_size = 4;
static const int fd = 44100;
static const size_t frame_size = 588U;
static const size_t track_size_A = 0x4000U;
static const size_t silence_size_A = 2250U;
static const size_t silence_strip_count_A = 3U; // Odd number
static const size_t pregap_size_A = 75U; // 1s pregap for 1st track
static const size_t tracks_num = 16U;
static const char *performer = "Tone generator";

trk_index_t calculate_index(const size_t offset);
int generate_image(const char *base_name);
int write_header(FILE *toc);
int write_track(const int trk_i, const size_t pregap, size_t *pos, FILE *cdimg, FILE *toc, const char *dataname);
int write_silence(const int trk_i, size_t *pos, FILE *cdimg, FILE *toc, const char *dataname);

int main (int argc, char **argv)
{
  int ret = CD_OK;

  if (2 == argc)
  {
    ret = generate_image(argv[1]);
  }
  else
  {
    fprintf(stderr, "Incorrect arg.\nUsage: %s outbasename\n\n", argv[0]);
    ret = CD_ERR_ARG;
  }

  return ret;
}

trk_index_t calculate_index(const size_t offset_s)
{
  trk_index_t ret;

  const size_t offset = offset_s / frame_size;
  const size_t deviation = offset_s % frame_size;

  if (deviation)
  {
    fprintf(stderr, "Calculated index deviation %lld\n", (long long int)deviation);
  }

  const size_t div_m = 4500U;
  const size_t div_s = 75U;

  ret.m = offset / div_m;
  ret.s = (offset % div_m) / div_s;
  ret.f = (offset % div_m % div_s);

  return ret;
}

int generate_image(const char *base_name)
{
  const size_t pregap_size = pregap_size_A * frame_size;
  int ret = -10;
  size_t pos = 0;
  char *cdimg_name = malloc(strlen(base_name) + 4);
  char *toc_name = malloc(strlen(base_name) + 4);
  strcpy(cdimg_name, base_name);
  strcpy(toc_name, base_name);
  strcat(cdimg_name, ".cdr");
  strcat(toc_name, ".toc");

  if ((NULL != cdimg_name) && (NULL != toc_name)) {
    FILE *cdimg = fopen(cdimg_name, "wb");
    FILE *toc = fopen(toc_name, "wt");

    if (cdimg && toc) {
      ret = write_header(toc);

      if (CD_OK == ret) {
        size_t trk_i = 0U;
        for (trk_i = 1; trk_i <= tracks_num; trk_i++)
        {
          ret = write_track(trk_i, (1 < trk_i ? 0U : pregap_size), &pos, cdimg, toc, cdimg_name);
          if (CD_OK != ret) {
            break;
          }
        }
        if (CD_OK == ret)
        {
          ret = write_silence(trk_i, &pos, cdimg, toc, cdimg_name);
        }
      }
      fclose(cdimg);
      fclose(toc);
    }
    else
    {
      fprintf(stderr, "Error opening files!\nTerminating!!!\n\n");
      exit(1);
    }
  }
  else
  {
    fprintf(stderr, "Error allocating memory\n\n");
    ret = CD_ERR_MEM;
  }

  fprintf(stderr, "\nDone.\n\n");


  free(cdimg_name);
  free(toc_name);

  return ret;
}

int write_header(FILE *toc)
{
  int ret = CD_OK;
  const char *title = "Sixteen pure tones one octave step locked to FD";
  const char *message = "All tone frequencies are fraction of FD to avoid beating";
  const char *genre = "{ 0, 25}";

  int pr_ret = fprintf(toc,
    "CD_DA\n"
    "CD_TEXT {\n"
    "  LANGUAGE_MAP {\n"
    "    0: 9\n"
    "  }\n"
    "  LANGUAGE 0 {\n"
    "    TITLE \"%s\"\n"
    "    PERFORMER \"%s\"\n"
    "    MESSAGE \"%s\"\n"
    "    GENRE \"%s\"\n"
    "  }\n"
    "}\n\n",
    title,
    performer,
    message,
    genre
    );

  if (0 > pr_ret) {
    fprintf(stderr, "Write error (toc): %s!\n\n", strerror(errno));
    ret = CD_ERR_FILE;
  }

  return ret;
}

int write_track(const int trk_i, const size_t pregap, size_t *pos, FILE *cdimg, FILE *toc, const char *dataname)
{
  int ret = CD_OK;
  double freq = 0.0;
  int div = 0;
  const size_t begin_pregap = *pos;
  const size_t begin_pos = *pos + pregap;
  const int begin_frame = begin_pos / frame_size;
  const size_t end = begin_pos + (track_size_A * frame_size);
  const size_t track_length = end - begin_pregap;

  fprintf(stderr, "===\nwrite_track: trk_i=%d, pregap=%lu, *pos=%lu\n", trk_i, pregap, *pos);

  // Write a pregap if any
  if ((CD_OK == ret) && (0 < pregap))
  {
    const size_t sample_size = 4;
    char *pregap_buf = malloc(sample_size);
    if (NULL != pregap_buf)
    {
      memset (pregap_buf, 0, sample_size);
      for (size_t i = 0U; i < pregap ; i++)
      {
        size_t chunks_wr = fwrite(pregap_buf, sample_size, 1, cdimg);
        if (1 == chunks_wr)
        {
          (*pos)++;
        }
        else
        {
          fprintf(stderr, "Write error (gap): %s!\n\n", strerror(errno));
          ret = CD_ERR_FILE;
          break;
        }
      }
    }
    else
    {
      fprintf(stderr, "Memory allocation error(gap): %s!\n\n", strerror(errno));
      ret = CD_ERR_MEM;
    }
    free(pregap_buf);
  }

  // Write wave data
  if (CD_OK == ret)
  {
    size_t buf_len = (1 << (tracks_num - trk_i + 1));
    size_t halflen =  buf_len / 2U;
    size_t bufsize = halflen * 2U * sample_size;
    fprintf(stderr, "Track %02d: buf_len:%lu halflen:%lu bufsize:%lu\n", trk_i, buf_len, halflen, bufsize);
    uint8_t *buf = malloc(bufsize);
    sample_t *sam = malloc(sizeof(sample_t) * buf_len);

    freq = (double)fd / (double)buf_len;
    div = (int)buf_len;

    if (buf && sam)
    {
      double radpos = M_PI / halflen / 2;

      memset(buf, 0, bufsize);

      for (size_t i = 0; i < halflen; i++)
      {
        const int base_i = 0x8000;
        const double base_d = (double)base_i;
        const double half_d = 0.5;
        double dval = sin(radpos) * (base_d - half_d);
        double dval_neg = -dval;
        int val1 = (int)(dval + base_d);
        int val2 = (int)(dval_neg + base_d);
        val1 -= base_i;
        val2 -= base_i;
        if (-1 != (val1 + val2)) {
          fprintf(stderr, CD_WARN "Values are not symmetrical to neutral level 0.5: i=%d val1=%5d val2=%5d\n", (int)i, val1, val2);
        }

        sam[i].s.l = (uint16_t)val1;
        sam[i+halflen].s.l = (uint16_t)val2;
        sam[i].s.r = (uint16_t)val1;
        sam[i+halflen].s.r = (uint16_t)val2;

        radpos += (M_PI / halflen);
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

      while(end > *pos)
      {
        size_t chunks_wr = fwrite(buf, bufsize, 1, cdimg);
        if (1 == chunks_wr)
        {
          (*pos) += buf_len;
        }
        else
        {
          fprintf(stderr, "Write error (data): %s!\n\n", strerror(errno));
          ret = CD_ERR_FILE;
          break;
        }
      }
    }
    else
    {
      fprintf(stderr, "Memory allocation error(data): %s!\n\n", strerror(errno));
      ret = CD_ERR_MEM;
    }

    free(buf);
    free(sam);
  }
  
  const size_t next_pos = *pos;
  const int next_frame = next_pos / frame_size;
  const int begin_dev = (begin_frame * frame_size) - (int)begin_pos;
  const int next_dev = (next_frame * frame_size) - (int)next_pos;

  fprintf(stderr, "Track %02d: Position: (c:%10lu | n:%10lu) Deviation: (c:%4d | n:%4d) Frames: (c:%7d | n:%7d)\n",
      trk_i, begin_pos, next_pos, begin_dev, next_dev, begin_frame, next_frame);

  // Wtite TOC entry
  if (CD_OK == ret)
  {
    char title[200];
    char message[200];
    char pregap_line[80];

    snprintf(title, sizeof(title), "Tone %21.15f Hz (0 dB)", freq);
    snprintf(message, sizeof(message), "FD (%d Hz) divided by %2d%s", fd, div, (2.01 < ((double)fd / freq)) ? "" : " (Frequency outside filter range)");

    trk_index_t pre = calculate_index(pregap);
    snprintf(pregap_line, sizeof(pregap_line), "START %02d:%02d:%02d\n", (int)pre.m, (int)pre.s, (int)pre.f);

    int pr_ret = fprintf(toc,
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
      "FILE \"%s\" %d %d\n"
      "%s\n",
      trk_i,
      title,
      performer,
      message,
      dataname, (int)begin_pregap, (int)track_length,
      pregap ? pregap_line : ""
      );
    if (0 > pr_ret) {
      fprintf(stderr, "Write error (toc): %s!\n\n", strerror(errno));
      ret = CD_ERR_FILE;
    }
  }

  return ret;
}

int write_silence(const int trk_i, size_t *pos, FILE *cdimg, FILE *toc, const char *dataname)
{
  int ret = CD_OK;
  const size_t begin_pos = *pos;
  const int begin_frame = begin_pos / frame_size;
  const size_t end = begin_pos + (silence_size_A * silence_strip_count_A * frame_size);
  const size_t track_length = end - begin_pos;
  const size_t index_entries_initial_size = 0x100U;

  fprintf(stderr, "===\nwrite_silence: trk_i=%d, *pos=%lu\n", trk_i, *pos);

  char *index_entries = malloc(index_entries_initial_size);
  if (index_entries)
  {
    memset(index_entries, 0, index_entries_initial_size);
  }
  else
  {
    fprintf(stderr, "Memory allocation error(silence): %s!\n\n", strerror(errno));
    ret = CD_ERR_MEM;
  }

  // Write wave data
  if (CD_OK == ret)
  {
    size_t buf_len = 2U;
    const size_t chunk_cnt = frame_size / buf_len;
    size_t bufsize = buf_len * sample_size;
    fprintf(stderr, "Track %02d: buf_len:%lu bufsize:%lu\n", trk_i, buf_len, bufsize);
    uint8_t *buf = malloc(bufsize);
    sample_t *sam = malloc(sizeof(sample_t) * buf_len);

    if (buf && sam && index_entries)
    {
      memset(buf, 0, bufsize);

      for (size_t si = 0; si < silence_strip_count_A; si++)
      {
        const size_t index_pos = *pos - begin_pos;
        trk_index_t idx = calculate_index(index_pos);
        if (0U < index_pos) {
          char index[100];
          snprintf(index, sizeof(index), "INDEX %02d:%02d:%02d\n", (int)(idx.m), (int)(idx.s), (int)(idx.f));
          index_entries = realloc(index_entries, strlen(index_entries) + strlen(index) + 1U);
          if (index_entries)
          {
            strcat(index_entries, index);
          }
          else
          {
            fprintf(stderr, "Memory re-allocation error(silence): %s!\n\n", strerror(errno));
            ret = CD_ERR_MEM;
          }
        }


        sam[0].s.l = 0U;
        sam[1].s.l = (si % 2U) ? ((uint16_t)(-1)) : 0U;
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
          for(size_t i = 0; i < chunk_cnt; i++)
          {
            size_t chunks_wr = fwrite(buf, bufsize, 1, cdimg);
            if (1 == chunks_wr)
            {
              (*pos) += buf_len;
            }
            else
            {
              fprintf(stderr, "Write error (silence): %s!\n\n", strerror(errno));
              ret = CD_ERR_FILE;
              break;
            }
          }
        }
      }
    }
    else
    {
      fprintf(stderr, "Memory allocation error(silence): %s!\n\n", strerror(errno));
      ret = CD_ERR_MEM;
    }

    free(buf);
    free(sam);
  }
  
  const size_t next_pos = *pos;
  const int next_frame = next_pos / frame_size;
  const int begin_dev = (begin_frame * frame_size) - (int)begin_pos;
  const int next_dev = (next_frame * frame_size) - (int)next_pos;

  fprintf(stderr, "Track %02d: Position: (c:%10lu | n:%10lu) Deviation: (c:%4d | n:%4d) Frames: (c:%7d | n:%7d)\n",
      trk_i, begin_pos, next_pos, begin_dev, next_dev, begin_frame, next_frame);

  // Wtite TOC entry
  if (CD_OK == ret)
  {
    char title[200];
    char message[200];

    double strip_duration = ((double)silence_size_A * (double)frame_size) / (double)fd;

    snprintf(title, sizeof(title), "Silence");
    snprintf(message, sizeof(message), "Repeating %1.1f seconds constant zero level and %1.1f seconds clapping silence",
      strip_duration, strip_duration);

    int pr_ret = fprintf(toc,
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
      "FILE \"%s\" %d %d\n"
      "%s\n",
      trk_i,
      title,
      performer,
      message,
      dataname, (int)begin_pos, (int)track_length,
      index_entries
      );
    if (0 > pr_ret) {
      fprintf(stderr, "Write error (toc): %s!\n\n", strerror(errno));
      ret = CD_ERR_FILE;
    }
  }

  free(index_entries);

  return ret;
}

