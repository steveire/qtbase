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

#include "lalr.h"

#include <limits.h>

#include <algorithm>

#define QLALR_NO_DEBUG_NULLABLES
#define QLALR_NO_DEBUG_LOOKBACKS
#define QLALR_NO_DEBUG_DIRECT_READS
#define QLALR_NO_DEBUG_READS
#define QLALR_NO_DEBUG_INCLUDES
#define QLALR_NO_DEBUG_LOOKAHEADS

QT_BEGIN_NAMESPACE
QTextStream qerr (stderr, QIODevice::WriteOnly);
QTextStream qout (stdout, QIODevice::WriteOnly);

bool operator < (Name a, Name b)
{
    return *a < *b;
}

bool operator < (ItemPointer a, ItemPointer b)
{
    return &*a < &*b;
}

bool operator < (StatePointer a, StatePointer b)
{
  return &*a < &*b;
}
QT_END_NAMESPACE

bool Read::operator < (const Read &other) const
{
  if (state == other.state)
    return nt < other.nt;

  return state < other.state;
}

bool Include::operator < (const Include &other) const
{
  if (state == other.state)
    return nt < other.nt;

  return state < other.state;
}

bool Lookback::operator < (const Lookback &other) const
{
  if (state == other.state)
    return nt < other.nt;

  return state < other.state;
}

QTextStream &operator << (QTextStream &out, const Name &n)
{
  return out << *n;
}

QTextStream &operator << (QTextStream &out, const Rule &r)
{
  out << *r.lhs << " ::=";

  for (auto name = r.rhs.begin (); name != r.rhs.end (); ++name)
    out << " " << **name;

  return out;
}

QTextStream &operator << (QTextStream &out, const NameSet &ns)
{
  out << "{";

  for (auto n = ns.begin (); n != ns.end (); ++n)
    {
      if (n != ns.begin ())
        out << ", ";

      out << *n;
    }

  return out << "}";
}

Item Item::next () const
{
  Q_ASSERT (! isReduceItem ());

  Item n;
  n.rule = rule;
  n.dot = dot;
  ++n.dot;

  return n;
}

QTextStream &operator << (QTextStream &out, const Item &item)
{
  auto r = item.rule;

  out << *r->lhs << ":";
  for (auto name = r->rhs.begin (); name != r->rhs.end (); ++name)
    {
      out << " ";

      if (item.dot == name)
        out << ". ";

      out << **name;
    }

  if (item.isReduceItem ())
    out << " .";

  return out;
}

State::State (Grammar *g):
  defaultReduce (g->rules.end ())
{
}

QPair<ItemPointer, bool> State::insert (const Item &item)
{
  auto it = std::find (kernel.begin (), kernel.end (), item);

  if (it != kernel.end ())
    return qMakePair (it, false);

  return qMakePair (kernel.insert (it, item), true);
}

QPair<ItemPointer, bool> State::insertClosure (const Item &item)
{
  auto it = std::find (closure.begin (), closure.end (), item);

  if (it != closure.end ())
    return qMakePair (it, false);

  return qMakePair (closure.insert (it, item), true);
}


/////////////////////////////////////////////////////////////
// Grammar
/////////////////////////////////////////////////////////////
Grammar::Grammar ():
    start (names.end ())
{
  expected_shift_reduce = 0;
  expected_reduce_reduce = 0;
  current_prec = 0;
  current_assoc = NonAssoc;

  table_name = QLatin1String ("parser_table");

  tk_end = intern ("$end");
  terminals.insert (tk_end);
  spells.insert (tk_end, "end of file");

  /*tk_error= terminals.insert (intern ("error"))*/;
}

Name Grammar::intern (const QString &id)
{
  auto name = std::find (names.begin (), names.end (), id);

  if (name == names.end ())
    name = names.insert (names.end (), id);

  return name;
}

void Grammar::buildRuleMap ()
{
  NameSet undefined;
  for (auto rule = rules.begin (); rule != rules.end (); ++rule)
    {
      for (auto it = rule->rhs.begin (); it != rule->rhs.end (); ++it)
        {
          auto name = *it;
          if (isTerminal (name) || declared_lhs.find (name) != declared_lhs.end ()
              || undefined.find (name) != undefined.end ())
            continue;

          undefined.insert(name);
          fprintf (stderr, "*** Warning. Symbol `%s' is not defined\n", qPrintable (*name));
        }

      rule_map.insert (rule->lhs, rule);
    }
}

void Grammar::buildExtendedGrammar ()
{
  accept_symbol = intern ("$accept");
  goal = rules.insert (rules.end (), Rule ());
  goal->lhs = accept_symbol;
  goal->rhs.push_back (start);
  goal->rhs.push_back (tk_end);

  non_terminals.insert (accept_symbol);
}

struct Nullable: public std::unary_function<Name, bool>
{
  Automaton *_M_automaton;

  Nullable (Automaton *aut):
    _M_automaton (aut) {}

  bool operator () (Name name) const
  { return _M_automaton->nullables.find (name) != _M_automaton->nullables.end (); }
};

Automaton::Automaton (Grammar *g):
  _M_grammar (g),
  start (states.end ())
{
}

int Automaton::id (RulePointer rule)
{
  return 1 + std::distance (_M_grammar->rules.begin (), rule);
}

int Automaton::id (Name name)
{
  return std::distance (_M_grammar->names.begin (), name);
}

int Automaton::id (StatePointer state)
{
  return std::distance (states.begin (), state);
}

void Automaton::build ()
{
  Item item;
  item.rule = _M_grammar->goal;
  item.dot = _M_grammar->goal->rhs.begin ();

  State tmp (_M_grammar);
  tmp.insert (item);
  start = internState (tmp).first;

  closure (start);

  buildNullables ();
  buildLookbackSets ();
  buildReads ();
  buildIncludesAndFollows ();
  buildLookaheads ();
  buildDefaultReduceActions ();
}

void Automaton::buildNullables ()
{
  auto changed = true;

  while (changed)
    {
      changed = false;

      for (auto rule = _M_grammar->rules.begin (); rule != _M_grammar->rules.end (); ++rule)
        {
          auto nn = std::find_if (rule->rhs.begin (), rule->rhs.end (), std::not1 (Nullable (this)));

          if (nn == rule->rhs.end ())
            changed |= nullables.insert (rule->lhs).second;
        }
    }

#ifndef QLALR_NO_DEBUG_NULLABLES
  qerr << "nullables = {" << nullables << endl;
#endif
}

QPair<StatePointer, bool> Automaton::internState (const State &state)
{
  auto it = std::find (states.begin (), states.end (), state);

  if (it != states.end ())
    return qMakePair (it, false);

  return qMakePair (states.insert (it, state), true);
}

struct _Bucket
{
  QLinkedList<ItemPointer> items;

  void insert (ItemPointer item)
  { items.push_back (item); }

  State toState (Automaton *aut)
  {
    State st (aut->_M_grammar);

    for (auto item = items.begin (); item != items.end (); ++item)
      st.insert ((*item)->next ());

    return st;
  }
};

void Automaton::closure (StatePointer state)
{
  if (! state->closure.empty ()) // ### not true.
    return;

  typedef QMap<Name, _Bucket> bucket_map_type;

  bucket_map_type buckets;
  QStack<ItemPointer> working_list;

  for (auto item = state->kernel.begin (); item != state->kernel.end (); ++item)
    working_list.push (item);

  state->closure = state->kernel;

  while (! working_list.empty ())
    {
      auto item = working_list.top ();
      working_list.pop ();

      if (item->isReduceItem ())
        continue;

      buckets [*item->dot].insert (item);

      if (_M_grammar->isNonTerminal (*item->dot))
        {
          const auto range = qAsConst(_M_grammar->rule_map).equal_range(*item->dot);
          for (auto it = range.first; it != range.second; ++it)
            {
              const RulePointer &rule = *it;
              Item ii;
              ii.rule = rule;
              ii.dot = rule->rhs.begin ();

              auto r = state->insertClosure (ii);

              if (r.second)
                working_list.push (r.first);
            }
        }
    }

  QList<StatePointer> todo;

  for (auto bucket = buckets.begin (); bucket != buckets.end (); ++bucket)
    {
      auto r = internState (bucket->toState (this));

      auto target = r.first;

      if (r.second)
        todo.push_back (target);

      state->bundle.insert (bucket.key(), target);
    }

  while (! todo.empty ())
    {
      closure (todo.front ());
      todo.pop_front ();
    }
}

void Automaton::buildLookbackSets ()
{
  for (auto p = states.begin (); p != states.end (); ++p)
    {
      for (auto a = p->bundle.begin (); a != p->bundle.end (); ++a)
        {
          auto A = a.key ();

          if (! _M_grammar->isNonTerminal (A))
            continue;

          const auto range = qAsConst(_M_grammar->rule_map).equal_range(A);
          for (auto it = range.first; it != range.second; ++it)
            {
              const RulePointer &rule = *it;
              auto q = p;

              for (auto dot = rule->rhs.begin (); dot != rule->rhs.end (); ++dot)
                q = q->bundle.value (*dot, states.end ());

              Q_ASSERT (q != states.end ());

              auto item = q->closure.begin ();

              for (; item != q->closure.end (); ++item)
                {
                  if (item->rule == rule && item->dot == item->end_rhs ())
                    break;
                }

              if (item == q->closure.end ())
                {
                  Q_ASSERT (q == p);
                  Q_ASSERT (rule->rhs.begin () == rule->rhs.end ());

                  for (item = q->closure.begin (); item != q->closure.end (); ++item)
                    {
                      if (item->rule == rule && item->dot == item->end_rhs ())
                        break;
                    }
                }

              Q_ASSERT (item != q->closure.end ());

              lookbacks.insert (item, Lookback (p, A));

#ifndef QLALR_NO_DEBUG_LOOKBACKS
              qerr << "*** (" << id (q) << ", " << *rule << ") lookback (" << id (p) << ", " << *A << ")" << endl;
#endif
            }
        }
    }
}

void Automaton::buildDirectReads ()
{
  for (auto q = states.begin (); q != states.end (); ++q)
    {
      for (auto a = q->bundle.begin (); a != q->bundle.end (); ++a)
        {
          if (! _M_grammar->isNonTerminal (a.key ()))
            continue;

          auto r = a.value ();

          for (auto z = r->bundle.begin (); z != r->bundle.end (); ++z)
            {
              auto sym = z.key ();

              if (! _M_grammar->isTerminal (sym))
                continue;

              q->reads [a.key ()].insert (sym);
            }
        }

#ifndef QLALR_NO_DEBUG_DIRECT_READS
      for (QMap<Name, NameSet>::iterator dr = q->reads.begin (); dr != q->reads.end (); ++dr)
        qerr << "*** DR(" << id (q) << ", " << dr.key () << ") = " << dr.value () << endl;
#endif
    }
}

void Automaton::buildReadsDigraph ()
{
  for (auto q = states.begin (); q != states.end (); ++q)
    {
      for (auto a = q->bundle.begin (); a != q->bundle.end (); ++a)
        {
          if (! _M_grammar->isNonTerminal (a.key ()))
            continue;

          auto r = a.value ();

          for (auto z = r->bundle.begin (); z != r->bundle.end (); ++z)
            {
              auto sym = z.key ();

              if (! _M_grammar->isNonTerminal(sym) || nullables.find (sym) == nullables.end ())
                continue;

              auto source = ReadsGraph::get (Read (q, a.key ()));
              auto target = ReadsGraph::get (Read (r, sym));

              source->insertEdge (target);

#ifndef QLALR_NO_DEBUG_READS
              qerr << "*** ";
              dump (qerr, source);
              qerr << " reads ";
              dump (qerr, target);
              qerr << endl;
#endif
            }
        }
    }
}

void Automaton::buildReads ()
{
  buildDirectReads ();
  buildReadsDigraph ();

  _M_reads_dfn = 0;

  for (auto node = ReadsGraph::begin_nodes (); node != ReadsGraph::end_nodes (); ++node)
    {
      if (! node->root)
        continue;

      visitReadNode (node);
    }

  for (auto node = ReadsGraph::begin_nodes (); node != ReadsGraph::end_nodes (); ++node)
    visitReadNode (node);
}

void Automaton::visitReadNode (ReadNode node)
{
  if (node->dfn != 0)
    return; // nothing to do

  auto N = node->dfn = ++_M_reads_dfn;
  _M_reads_stack.push (node);

#ifndef QLALR_NO_DEBUG_INCLUDES
  // qerr << "*** Debug. visit node (" << id (node->data.state) << ", " << node->data.nt << ")  N = " << N << endl;
#endif

  for (auto edge = node->begin (); edge != node->end (); ++edge)
    {
      auto r = *edge;

      visitReadNode (r);

      node->dfn = qMin (N, r->dfn);

      NameSet &dst = node->data.state->reads [node->data.nt];
      NameSet &src = r->data.state->reads [r->data.nt];
      dst.insert (src.begin (), src.end ());
    }

  if (node->dfn == N)
    {
      auto tos = _M_reads_stack.top ();

      do {
        tos = _M_reads_stack.top ();
        _M_reads_stack.pop ();
        tos->dfn = INT_MAX;
      } while (tos != node);
    }
}

void Automaton::buildIncludesAndFollows ()
{
  for (auto p = states.begin (); p != states.end (); ++p)
    p->follows = p->reads;

  buildIncludesDigraph ();

  _M_includes_dfn = 0;

  for (auto node = IncludesGraph::begin_nodes (); node != IncludesGraph::end_nodes (); ++node)
    {
      if (! node->root)
        continue;

      visitIncludeNode (node);
    }

  for (auto node = IncludesGraph::begin_nodes (); node != IncludesGraph::end_nodes (); ++node)
    visitIncludeNode (node);
}

void Automaton::buildIncludesDigraph ()
{
  for (auto pp = states.begin (); pp != states.end (); ++pp)
    {
      for (auto a = pp->bundle.begin (); a != pp->bundle.end (); ++a)
        {
          auto name = a.key ();

          if (! _M_grammar->isNonTerminal (name))
            continue;

          const auto range = qAsConst(_M_grammar->rule_map).equal_range(name);
          for (auto it = range.first; it != range.second; ++it)
            {
              const RulePointer &rule = *it;
              auto p = pp;

              for (auto A = rule->rhs.begin (); A != rule->rhs.end (); ++A)
                {
                  auto dot = A;
                  ++dot;

                  if (_M_grammar->isNonTerminal (*A) && dot == rule->rhs.end ())
                    {
                      // found an include edge.
                      auto target = IncludesGraph::get (Include (pp, name));
                      auto source = IncludesGraph::get (Include (p, *A));

                      source->insertEdge (target);

#ifndef QLALR_NO_DEBUG_INCLUDES
                      qerr << "*** (" << id (p) << ", " << *A << ") includes (" << id (pp) << ", " << *name << ")" << endl;
#endif // QLALR_NO_DEBUG_INCLUDES

                      continue;
                    }

                  p = p->bundle.value (*A);

                  if (! _M_grammar->isNonTerminal (*A))
                    continue;

                  auto first_not_nullable = std::find_if (dot, rule->rhs.end (), std::not1 (Nullable (this)));
                  if (first_not_nullable != rule->rhs.end ())
                    continue;

                  // found an include edge.
                  auto target = IncludesGraph::get (Include (pp, name));
                  auto source = IncludesGraph::get (Include (p, *A));

                  source->insertEdge (target);

#ifndef QLALR_NO_DEBUG_INCLUDES
                  qerr << "*** (" << id (p) << ", " << *A << ") includes (" << id (pp) << ", " << *name << ")" << endl;
#endif // QLALR_NO_DEBUG_INCLUDES
                }
            }
        }
    }
}

void Automaton::visitIncludeNode (IncludeNode node)
{
  if (node->dfn != 0)
    return; // nothing to do

  auto N = node->dfn = ++_M_includes_dfn;
  _M_includes_stack.push (node);

#ifndef QLALR_NO_DEBUG_INCLUDES
  // qerr << "*** Debug. visit node (" << id (node->data.state) << ", " << node->data.nt << ")  N = " << N << endl;
#endif

  for (auto edge = node->begin (); edge != node->end (); ++edge)
    {
      auto r = *edge;

      visitIncludeNode (r);

      node->dfn = qMin (N, r->dfn);

#ifndef QLALR_NO_DEBUG_INCLUDES
      qerr << "*** Merge. follows";
      dump (qerr, node);
      qerr << " += follows";
      dump (qerr, r);
      qerr << endl;
#endif

      NameSet &dst = node->data.state->follows [node->data.nt];
      NameSet &src = r->data.state->follows [r->data.nt];

      dst.insert (src.begin (), src.end ());
    }

  if (node->dfn == N)
    {
      auto tos = _M_includes_stack.top ();

      do {
        tos = _M_includes_stack.top ();
        _M_includes_stack.pop ();
        tos->dfn = INT_MAX;
      } while (tos != node);
    }
}

void Automaton::buildLookaheads ()
{
  for (auto p = states.begin (); p != states.end (); ++p)
    {
      for (auto item = p->closure.begin (); item != p->closure.end (); ++item)
        {
          const auto range = qAsConst(lookbacks).equal_range(item);
          for (auto it = range.first; it != range.second; ++it)
            {
              const Lookback &lookback = *it;
              auto q = lookback.state;

#ifndef QLALR_NO_DEBUG_LOOKAHEADS
              qerr << "(" << id (p) << ", " << *item->rule << ") lookbacks ";
              dump (qerr, lookback);
              qerr << " with follows (" << id (q) << ", " << lookback.nt << ") = " << q->follows [lookback.nt] << endl;
#endif

              lookaheads [item].insert (q->follows [lookback.nt].begin (), q->follows [lookback.nt].end ());
            }
        }

      // propagate the lookahead in the kernel
      auto k = p->kernel.begin ();
      auto c = p->closure.begin ();

      for (; k != p->kernel.end (); ++k, ++c)
        lookaheads [k] = lookaheads [c];
    }
}

void Automaton::buildDefaultReduceActions ()
{
  for (auto state = states.begin (); state != states.end (); ++state)
    {
      auto def = state->closure.end ();
      auto size = -1;

      for (auto item = state->closure.begin (); item != state->closure.end (); ++item)
        {
          if (item->dot != item->end_rhs ())
            continue;

          auto la = lookaheads.value (item).size ();
          if (def == state->closure.end () || la > size)
            {
              def = item;
              size = la;
            }
        }

      if (def != state->closure.end ())
        {
          Q_ASSERT (size >= 0);
          state->defaultReduce = def->rule;
        }
    }
}

void Automaton::dump (QTextStream &out, IncludeNode incl)
{
  out << "(" << id (incl->data.state) << ", " << incl->data.nt << ")";
}

void Automaton::dump (QTextStream &out, ReadNode rd)
{
  out << "(" << id (rd->data.state) << ", " << rd->data.nt << ")";
}

void Automaton::dump (QTextStream &out, const Lookback &lp)
{
  out << "(" << id (lp.state) << ", " << lp.nt << ")";
}
