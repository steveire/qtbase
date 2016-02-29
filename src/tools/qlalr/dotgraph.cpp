/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the utils of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "dotgraph.h"

#include "lalr.h"

#include <QtCore/qtextstream.h>

DotGraph::DotGraph(QTextStream &o):
  out (o)
{
}

void DotGraph::operator () (Automaton *aut)
{
  auto g = aut->_M_grammar;

  out << "digraph {" << endl << endl;

  out << "subgraph Includes {" << endl;
  for (auto incl = Automaton::IncludesGraph::begin_nodes ();
       incl != Automaton::IncludesGraph::end_nodes (); ++incl)
    {
      for (auto edge = incl->begin (); edge != incl->end (); ++edge)
        {
          out << "\t\"(" << aut->id (incl->data.state) << ", " << incl->data.nt << ")\"";
          out << "\t->\t";
          out << "\"(" << aut->id ((*edge)->data.state) << ", " << (*edge)->data.nt << ")\"\t";
          out << "[label=\"" << incl->data.state->follows [incl->data.nt] << "\"]";
          out << endl;
        }
    }
  out << "}" << endl << endl;


  out << "subgraph LRA {" << endl;
  //out << "node [shape=record];" << endl << endl;

  for (auto q = aut->states.begin (); q != aut->states.end (); ++q)
    {
      auto state = aut->id (q);

      out << "\t" << state << "\t[shape=record,label=\"{";

      out << "<0> State " << state;

      auto index = 1;
      for (auto item = q->kernel.begin (); item != q->kernel.end (); ++item)
        out << "| <" << index++ << "> " << *item;

      out << "}\"]" << endl;

      for (auto a = q->bundle.begin (); a != q->bundle.end (); ++a)
        {
          auto clr = g->isTerminal (a.key ()) ? "blue" : "red";
          out << "\t" << state << "\t->\t" << aut->id (*a) << "\t[color=\"" << clr << "\",label=\"" << a.key () << "\"]" << endl;
        }
      out << endl;
    }

  out << "}" << endl;
  out << endl << endl << "}" << endl;
}
