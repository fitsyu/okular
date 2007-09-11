/***************************************************************************
 *   Copyright (C) 2004 by Enrico Ros <eros.kde@email.it>                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

// reimplementing this
#include "preferencesdialog.h"

#include <klocale.h>

// single config pages
#include "dlggeneral.h"
#include "dlgperformance.h"
#include "dlgaccessibility.h"
#include "dlgpresentation.h"
#include "dlgidentity.h"
#include "dlgdebug.h"

PreferencesDialog::PreferencesDialog( QWidget * parent, KConfigSkeleton * skeleton )
    : KConfigDialog( parent, "preferences", skeleton )
{
    m_general = new DlgGeneral( this );
    m_performance = new DlgPerformance( this );
    m_accessibility = new DlgAccessibility( this );
    m_presentation = new DlgPresentation( this );
    m_identity = new DlgIdentity( this );
    m_debug = new DlgDebug( this );

    addPage( m_general, i18n("General"), "okular", i18n("General Options") );
    addPage( m_accessibility, i18n("Accessibility"), "access", i18n("Accessibility Reading Aids") );
    addPage( m_performance, i18n("Performance"), "launch", i18n("Performance Tuning") );
    addPage( m_presentation, i18n("Presentation"), "kpresenter_kpr",
             i18n("Options for Presentation Mode") );
    addPage( m_identity, i18n("Identity"), "identity",
             i18n("Identity Settings") );
    addPage( m_debug, "Debug", "gear", "Debug options" );
}
