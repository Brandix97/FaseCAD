/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "projectpropertiesdialog.h"

#include "../configdialog.h"
#include "configpage/configpages.h"
#include "configpage/projectconfigpages.h"
#include "../TerminalStrip/ui/ConfigPage/terminalstripprojectconfigpage.h"

#include <QObject>

/**
	@brief ProjectPropertiesDialog::ProjectPropertiesDialog
	Default constructor
	@param project : project to edit properties
	@param parent : parent widget of this dialog
*/
ProjectPropertiesDialog::ProjectPropertiesDialog(QETProject *project, QWidget *parent)
{
	m_properties_dialog = new ConfigDialog (parent);
	m_properties_dialog -> setWindowTitle(QObject::tr("Project properties", "window title"));
	m_properties_dialog -> addPage(new ProjectMainConfigPage(project));

	NewDiagramPage *newDiagramPage = new NewDiagramPage(project,parent,this);
	m_properties_dialog -> addPage(newDiagramPage);

		//No automatic numbering page: FaseCAD names a device as it is
		//placed (DeviceNaming); the cables have their own window
		//(CableNumberingDialog)
	m_properties_dialog->addPage(new TerminalStripProjectConfigPage { project, parent });
}

/**
	@brief ProjectPropertiesDialog::~ProjectPropertiesDialog
*/
ProjectPropertiesDialog::~ProjectPropertiesDialog ()
{
	delete m_properties_dialog;
}

/**
	@brief ProjectPropertiesDialog::exec
	execute this dialog.
*/
void ProjectPropertiesDialog::exec()
{
	// ApplicationModal (not WindowModal) so no other window — including other
	// MDI subwindows — can dispatch events while the dialog holds raw
	// QETProject* pointers in its config pages.  WindowModal only blocks the
	// parent ProjectView; the rest of the MDI area stays live, which allowed
	// new_project / close_project to replace the project under the dialog
	// and cause a SIGSEGV.  See issue #527.
	m_properties_dialog->setWindowModality(Qt::ApplicationModal);
	m_properties_dialog -> exec();
}

/**
	@brief ProjectPropertiesDialog::setCurrentPage
	Change the current displayed page by p.
	@param p : page to display
*/
void ProjectPropertiesDialog::setCurrentPage(ProjectPropertiesDialog::Page p) {
		//The automatic numbering page is gone: the first page is shown
	if (p == Autonum) {
		return;
	}
	m_properties_dialog -> setCurrentPage(static_cast <int> (p));
}

/**
	@brief ProjectPropertiesDialog::changeToFolio
	Change the current displayed tab to folio tab.
*/
void ProjectPropertiesDialog::changeToFolio()
{
		//The automatic numbering page is gone: nothing to show
}

/**
	@brief ProjectPropertiesDialog::changeToCable
	Change the current displayed tab to the cable numbering tab.
	The caller is the cable tool, which opens this window when it finds
	that the project numbers its cables by no rule at all.
*/
void ProjectPropertiesDialog::changeToCable()
{
		//The automatic numbering page is gone: nothing to show
}

/**
	@brief ProjectPropertiesDialog::changeToElement
	Change the current displayed tab to the element numberings tab.
*/
void ProjectPropertiesDialog::changeToElement()
{
		//The automatic numbering page is gone: nothing to show
}
