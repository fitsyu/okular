/*
 * Copyright (c) 1994 Paul Vojta.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * NOTE:
 *	xdvi is based on prior work as noted in the modification history, below.
 */

/*
 * DVI previewer for X.
 *
 * Eric Cooper, CMU, September 1985.
 *
 * Code derived from dvi-imagen.c.
 *
 * Modification history:
 * 1/1986	Modified for X.10	--Bob Scheifler, MIT LCS.
 * 7/1988	Modified for X.11	--Mark Eichin, MIT
 * 12/1988	Added 'R' option, toolkit, magnifying glass
 *					--Paul Vojta, UC Berkeley.
 * 2/1989	Added tpic support	--Jeffrey Lee, U of Toronto
 * 4/1989	Modified for System V	--Donald Richardson, Clarkson Univ.
 * 3/1990	Added VMS support	--Scott Allendorf, U of Iowa
 * 7/1990	Added reflection mode	--Michael Pak, Hebrew U of Jerusalem
 * 1/1992	Added greyscale code	--Till Brychcy, Techn. Univ. Muenchen
 *					  and Lee Hetherington, MIT
 * 4/1994	Added DPS support, bounding box
 *					--Ricardo Telichevesky
 *					  and Luis Miguel Silveira, MIT RLE.
 */



#include "dvi_init.h"
#include "dviwin.h"



#include <kdebug.h>
#include <klocale.h>
#include <qbitmap.h> 
#include <qfileinfo.h>
#include <stdlib.h>


extern "C" {
#include "dvi.h"
}

#include "fontpool.h"
#include "glyph.h"
#include "oconfig.h"


void dvifile::process_preamble(void)
{
  command_pointer = dvi_Data;

  Q_UINT8 magic_number = readUINT8();
  if (magic_number != PRE) {
    errorMsg = i18n("The DVI file does not start with the preamble.");
    return;
  }
  magic_number =  readUINT8();
  if (magic_number != 2) {
    errorMsg = i18n("The DVI file contains the wrong version of DVI output for this program. "
		    "Hint: If you use the typesetting system Omega, you have to use a special "
		    "program, such as oxdvi.");
    return;
  }

  numerator     = readUINT32();
  denominator   = readUINT32();
  magnification = readUINT32();
  dimconv       = (((double) numerator * magnification) / ((double) denominator * 1000.0));
  // @@@@ This does not fit the description of dimconv in the header file!!!
  dimconv       = dimconv * (((long) pixels_per_inch)<<16) / 254000;

  // Read the generatorString (such as "TeX output ..." from the
  // DVI-File). The variable "magic_number" holds the length of the
  // string.
  char	job_id[300];
  magic_number = readUINT8();
  strncpy(job_id, (char *)command_pointer, magic_number);
  job_id[magic_number] = '\0';
  generatorString = job_id;
}


/** find_postamble locates the beginning of the postamble and leaves
    the file ready to start reading at that location. */

void dvifile::find_postamble(void)
{
  // Move backwards through the TRAILER bytes
  command_pointer = dvi_Data + size_of_file - 1;
  while((*command_pointer == TRAILER) && (command_pointer > dvi_Data))
    command_pointer--;
  if (command_pointer == dvi_Data) {
    errorMsg = i18n("The DVI file is badly corrupted. KDVI was not able to find the postamble.");
    return;
  }

  // And this is finally the pointer to the beginning of the postamble
  command_pointer -= 4;
  beginning_of_postamble = readUINT32();
  command_pointer  = dvi_Data + beginning_of_postamble;
}


void dvifile::read_postamble(void)
{
  Q_UINT8 magic_byte = readUINT8();
  if (magic_byte != POST) {
    errorMsg = i18n("The postamble does not begin with the POST command.");
    return;
  }
  last_page_offset = readUINT32();

  // Skip the numerator, denominator and magnification, the largest
  // box height and width and the maximal depth of the stack. These
  // are not used at the moment.
  command_pointer += 4 + 4 + 4 + 4 + 4 + 2;

  // The number of pages is more interesting for us.
  total_pages  = readUINT16();

  // As a next step, read the font definitions.
  Q_UINT8 cmnd = readUINT8();
  while (cmnd >= FNTDEF1 && cmnd <= FNTDEF4) {
    Q_UINT32 TeXnumber = readUINT(cmnd-FNTDEF1+1);
    Q_UINT32 checksum  = readUINT32();
    Q_UINT32 scale     = readUINT32();
    Q_UINT32 design    = readUINT32();
    Q_UINT16 len       = readUINT8() + readUINT8();

    char *fontname  = new char[len + 1];
    strncpy(fontname, (char *)command_pointer, len );
    fontname[len] = '\0';
    command_pointer += len;
    
#ifdef DEBUG_FONTS
    kdDebug() << "Postamble: define font \"" << fontname << "\" scale=" << scale << " design=" << design << endl;
#endif
    
    // Calculate the fsize as:  fsize = 0.001 * scale / design * magnification * MFResolutions[MetafontMode]
    struct font *fontp = font_pool->appendx(fontname, checksum, scale, design, 0.001*scale/design*magnification*MFResolutions[font_pool->getMetafontMode()], dimconv);
    
    // Insert font in dictionary and make sure the dictionary is big
    // enough.
    if (tn_table.size()-2 <= tn_table.count())
      // Not quite optimal. The size of the dictionary should be a
      // prime. I don't care.
      tn_table.resize(tn_table.size()*2); 
    tn_table.insert(TeXnumber, fontp);

    // Read the next command
    cmnd = readUINT8();
  }
  
  if (cmnd != POSTPOST) {
    errorMsg = i18n("The postamble contained a command other than FNTDEF.");
    return;
  }

  // Now we remove all those fonts from the memory which are no longer
  // in use.
  font_pool->release_fonts();
}

void dvifile::prepare_pages()
{
#ifdef DEBUG_DVIFILE
  kdDebug() << "prepare_pages" << endl;
#endif

  page_offset              = new Q_UINT32[total_pages+1];
  if (page_offset == 0) {
    kdError(4300) << "No memory for page list!" << endl;
    return;
  }

  page_offset[total_pages] = beginning_of_postamble;
  Q_UINT16 i               = total_pages-1;
  page_offset[i]           = last_page_offset;

  // Follow back pointers through pages in the DVI file, storing the
  // offsets in the page_offset table.
  // @@@@ ADD CHECK FOR CONSISTENCY !!! NEVER LET THE COMMAND PTR POINT OUTSIDE THE ALLOCATED MEM !!!!
  while (i > 0) {
    command_pointer  = dvi_Data + page_offset[i--];
    if (readUINT8() != BOP) {
      errorMsg = i18n("The page %1 does not start with the BOP command.").arg(i+1);
      return;
    }
    command_pointer += 10 * 4;
    page_offset[i] = readUINT32();
    if ((dvi_Data+page_offset[i] < dvi_Data)||(dvi_Data+page_offset[i] > dvi_Data+size_of_file))
      page_offset[i] = 0;
  }
}


dvifile::dvifile(QString fname, fontPool *pool, bool sourceSpecialMark=true)
{
#ifdef DEBUG_DVIFILE
  kdDebug() << "init_dvi_file: " << fname << endl;
#endif

  errorMsg    = QString::null;
  dvi_Data    = 0;
  page_offset = 0;
  font_pool   = pool;
  sourceSpecialMarker = sourceSpecialMark;

  QFile file(fname);
  filename = file.name();
  file.open( IO_ReadOnly );
  size_of_file = file.size();
  dvi_Data = new Q_UINT8[size_of_file];
  // Sets the end pointer for the bigEndianByteReader so that the
  // whole memory buffer is readable
  end_pointer = dvi_Data+size_of_file; 
  if (dvi_Data == 0) {
    kdError() << i18n("Not enough memory to load the DVI-file.");
    return;
  }
  file.readBlock((char *)dvi_Data, size_of_file);
  file.close();
  if (file.status() != IO_Ok) {
    kdError() << i18n("Could not load the DVI-file.");
    return;
  }

  tn_table.clear();
  
  process_preamble();
  find_postamble();
  read_postamble();
  prepare_pages();

  return;
}

dvifile::~dvifile()
{
#ifdef DEBUG_DVIFILE
  kdDebug() << "destroy dvi-file" << endl;
#endif

  if (dvi_Data != 0)
    delete [] dvi_Data;
  if (font_pool != 0)
    font_pool->mark_fonts_as_unused();
  if (page_offset != NULL)
    delete [] page_offset;
}
