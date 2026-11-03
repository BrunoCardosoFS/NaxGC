/*
NaxGC
Copyright (C) 2026 Bruno Cardoso <brunocardosofs@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <QGuiApplication>

int main(int argc, char **argv)
{
#if defined(__linux__)
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
		qputenv("QT_QPA_PLATFORM", "offscreen");
	}
#endif

	int res = 0;
	{
		QGuiApplication app(argc, argv);

		doctest::Context context;
		context.applyCommandLine(argc, argv);

		res = context.run();
		if (context.shouldExit()) {
			return res;
		}
	}

	return res;
}