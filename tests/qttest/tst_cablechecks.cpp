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
/*
	CableChecks -- what the cable manager marks on a cable:

	* a cable named, typed, wired and with its spare cores has nothing;
	* no name, or a name another cable has;
	* no type, said alone: there are no cores to count;
	* no core wired;
	* fewer spare cores than asked for, and 0 asks for nothing.
*/

#include "cablelist/cablechecks.h"

#include <QtTest>

class TstCableChecks : public QObject
{
		Q_OBJECT

	private:
		static CableChecks::Facts cable(int cores, int used)
		{
			CableChecks::Facts facts;
			facts.designation = QStringLiteral("W1");
			facts.core_count = cores;
			facts.used_cores = used;
			return facts;
		}

	private slots:
		void goodCableHasNothing();
		void nameProblems();
		void noTypeIsSaidAlone();
		void noCoreWired();
		void spareCores();
};

void TstCableChecks::goodCableHasNothing()
{
	QVERIFY(CableChecks::problems(cable(5, 3), 1).isEmpty());
	QVERIFY(CableChecks::problems(cable(5, 3), 2).isEmpty());
}

void TstCableChecks::nameProblems()
{
	CableChecks::Facts facts = cable(5, 3);
	facts.designation = QStringLiteral("  ");
	QCOMPARE(CableChecks::problems(facts, 1),
			 QStringList{CableChecks::tr("No name")});

	facts.designation = QStringLiteral("W1");
	facts.duplicate_designation = true;
	QCOMPARE(CableChecks::problems(facts, 1),
			 QStringList{CableChecks::tr("Name used by another cable")});
}

void TstCableChecks::noTypeIsSaidAlone()
{
	QCOMPARE(CableChecks::problems(cable(0, 0), 3),
			 QStringList{CableChecks::tr("No cable type")});
}

void TstCableChecks::noCoreWired()
{
	QCOMPARE(CableChecks::problems(cable(5, 0), 1),
			 QStringList{CableChecks::tr("No core wired")});
}

void TstCableChecks::spareCores()
{
	QCOMPARE(CableChecks::problems(cable(5, 5), 1),
			 QStringList{CableChecks::tr("No spare core")});
	QCOMPARE(CableChecks::problems(cable(5, 4), 2).size(), 1);
	QVERIFY(CableChecks::problems(cable(5, 4), 2).first().contains(
				QLatin1String("1")));
		//0 asks for no spare core at all
	QVERIFY(CableChecks::problems(cable(5, 5), 0).isEmpty());
}

QTEST_GUILESS_MAIN(TstCableChecks)
#include "tst_cablechecks.moc"
