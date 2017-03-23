/*
   Copyright 2015 Software Reliability Lab, ETH Zurich

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#include "tree.h"

#include <limits>
#include <queue>

#include "gflags/gflags.h"
#include "base/stringprintf.h"

DEFINE_string(ast_format, "SpiderMonkey", "Ast format of the analyzed programs. SpiderMonkey | Lombok");
DEFINE_int32(max_tree_size, 30000, "Skip trees with more nodes than this number.");

const int TreeNode::EMPTY_NODE_LABEL = -1;
const int TreeNode::UNKNOWN_LABEL = -2;

const TreeNode TreeNode::EMPTY_NODE{TreeNode::EMPTY_NODE_LABEL, TreeNode::EMPTY_NODE_LABEL, -1, -1, -1, -1, -1, -1};


std::ostream& operator<<(std::ostream& os, const TreeNode& node) {
  os << "node(" << node.type << " " << node.value << " " << node.parent << " " << node.left_sib << " " << node.right_sib << " " << node.first_child << " " << node.last_child << " " << node.child_index << ")";
  return os;
}

std::ostream& operator<<(std::ostream& os, const TreeSubstitution::Node& f) {
  os << "TreeSub::Node[type=" << f.type << ", value=" << f.value << ", first_child=" << f.first_child << ", right_sib=" << f.right_sib << "]";
  return os;
}


void TreeStorage::SubstituteNodeWithTree(int node_id, const TreeStorage& other) {
  ConstLocalEpsTreeTraversal reader(&other, 0);
  RemoveNodeChildren(node_id);
  LocalEpsTreeTraversal writer(this, node_id);
  auto read_it = reader.begin();
  auto write_it = writer.begin();
  while (!read_it.at_end()) {
    CHECK(!write_it.at_end());
    write_it->CopyNodeEps(read_it.node());
    ++read_it;
    ++write_it;
  }
}


void TreeStorage::DebugStringTraverse(std::string* s, int node, int max_depth, int depth, const StringSet* ss,
    bool tree_indentation, int highlighted_position, int last_node, int start_node) const {
  if (max_depth == depth) {
    s->append("...");
    return;
  }

  for (size_t i = 0; i <= nodes_.size(); ++i) {
    if (node > last_node) {
      return;
    }
    if (node >= start_node && node <= last_node) {
      if (!tree_indentation) {
        if (i > 0) s->append(" ");
        s->append("[");
      }
      if (nodes_[node].parent == TREEPOINTER_DEALLOCATED) {
        s->append("ERR ");
      }
      if (tree_indentation) {
        if (node == highlighted_position) {
          s->append("**");
        } else {
          s->append("  ");
        }
        for (int i = 0; i < depth; i++){
          s->append(" ");
        }
        StringAppendF(s, "%d ", node);
      }

      s->append(NodeToString(ss, node));

      if (tree_indentation) {
        StringAppendF(s, " | children: [%d..%d], siblings: [%d,%d]\n", nodes_[node].first_child, nodes_[node].last_child, nodes_[node].left_sib, nodes_[node].right_sib);
      }
    }

    if (nodes_[node].first_child >= 0) {
      if (!tree_indentation) {
        if (node >= start_node && node <= last_node) {
          s->append(" ");
        }
      }
      DebugStringTraverse(s, nodes_[node].first_child, max_depth, depth + 1, ss, tree_indentation, highlighted_position, last_node, start_node);
    }

    if (node >= start_node && node <= last_node) {
      if (!tree_indentation) {
        s->append("]");
      }

      if (node == last_node) {
        s->append("...");
      }
    }

    node = nodes_[node].right_sib;
    if (node < 0) break;

    if (i == nodes_.size()) s->append("CYCLE");
  }
}

void indent(std::string *s, int depth){
//  StringAppendF(s, "%*s",depth, "\t");
  for (int i = 0; i < depth; i++){
    s->append("   ");
  }
}

void appendCommaFormatted(std::string *s){
  // to make the indentation nice, if the comma is after newline put it on the previous line
  if (s->back() == '\n') {
    s->back() = ',';
    s->append(" \n");
  } else {
    s->append(", ");
  }
}

void TreeStorage::PrettyPrintTraverse(std::string* s, ConstLocalTreeTraversal& t, const StringSet* ss, int depth, int highlighted_position, bool is_highlighting) const {
if (FLAGS_ast_format == "SpiderMonkey") {
    PrettyPrintTraverseJS(s, t, ss,depth, highlighted_position, is_highlighting);
  } else if (FLAGS_ast_format == "Lombok") {
    PrettyPrintTraverseJava(s, t, ss,depth, highlighted_position, is_highlighting);
  } else {
    LOG(FATAL) << "Pretty printing not implemented for '" << FLAGS_ast_format << "'";
  }
}

void TreeStorage::PrettyPrintTraverseJava(std::string* s, ConstLocalTreeTraversal& t, const StringSet* ss, int depth, int highlighted_position, bool is_highlighting) const {
  int type = t.node().type;

  // Workaround for highlighting such that we dont need to rewrite the code below
  if (t.position() == highlighted_position && !is_highlighting){
    s->append(std::string(HighlightColors::GREEN));
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, true);
    s->append(std::string(HighlightColors::DEFAULT));
    return;
  }

  if (type == ss->findString("AnnotationDeclaration") || type == ss->findString("Annotation") || type == ss->findString("TypeVariable")) {
    return;
  }

  if (type == ss->findString("EnumDeclaration")) {
    return;
  }

  if (type == ss->findString("EmptyStatement")) {
    CHECK(!t.down_first_child());
    return;
  }

  if (type == ss->findString("SuperConstructorInvocation") || type == ss->findString("AlternateConstructorInvocation")) {
    indent(s, depth);
    int last_child = t.node().last_child;
    if (type == ss->findString("SuperConstructorInvocation")) {
      s->append("super(");
    } else if (type == ss->findString("AlternateConstructorInvocation")) {
      s->append("this(");
    } else {
      LOG(FATAL) << "unexpected type: " << ss->getString(type);
    }
    if (t.down_first_child()){
      do {
        PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
        if (t.position() != last_child) {
          s->append(", ");
        }
      } while (t.right());
      t.up();
    }

    s->append(");\n");
    return;
  }

  if (type == ss->findString("VariableDeclaration") || type == ss->findString("ExpressionStatement")) {
    indent(s, depth);
    if (t.down_first_child()){
      do {
        PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      } while (t.right());
      t.up();
    }

    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 1);
    s->append(";\n");
    return;
  }

  if (type == ss->findString("CompilationUnit")|| type == ss->findString("InstanceInitializer")  || type == ss->findString("StaticInitializer") || type == ss->findString("Modifiers") || type == ss->findString("TypeRefSignature")) {
    int last_child = t.node().last_child;
    if (t.down_first_child()){
      do {
        PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
        if (t.position() != last_child && type == ss->findString("Modifiers")) {
          s->append(" ");
        }
      } while (t.right());
      t.up();
    }
    return;
  }

  if (type == ss->findString("KeywordModifier") || type == ss->findString("Identifier") ||
      type == ss->findString("IntegralLiteral") || type == ss->findString("BooleanLiteral") || type == ss->findString("NullLiteral") ||
      type == ss->findString("StringLiteral") || type == ss->findString("FloatingPointLiteral") || type == ss->findString("CharLiteral") ||
      type == ss->findString("TypeReference") || type == ss->findString("VariableReference")) {
    s->append(ss->getString(t.node().value));
    return;
  }

  if (type == ss->findString("ClassDeclaration") || type == ss->findString("InterfaceDeclaration")) {
    CHECK(t.down_first_child());
    //Modifiers
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    //Class Name
    CHECK(t.right());
    if (type == ss->findString("ClassDeclaration")) {
      s->append(" class ");
    } else if (type == ss->findString("InterfaceDeclaration")) {
      s->append(" interface ");
    } else {
      LOG(FATAL) << "unexpected type: " << ss->getString(type);
    }
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);

    // extends, implements
    while(t.right()) {}
    //Body
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);

    t.up();
    return;
  }

  if (type == ss->findString("NormalTypeBody") || type == ss->findString("Block")) {
    s->append("{\n");
    if (t.down_first_child()){
      do {
        PrettyPrintTraverseJava(s, t, ss, depth + 1, highlighted_position, is_highlighting);
      } while (t.right());
      t.up();
    }
    indent(s, depth);
    s->append("}\n");
    return;
  }

  if (type == ss->findString("VariableDefinition")) {
    int last_pos = t.node().last_child;
    CHECK(t.down_first_child());
    // Modifiers
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(t.right());
    // Type
    s->append(" ");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(t.right());
    s->append(" ");
    // VariableDefinitionEntry
    do {
      // there can be multiple definitions
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      if (t.position() != last_pos) {
        s->append(", ");
      }
    } while (t.right());
    t.up();
    return;
  }

  if (type == ss->findString("VariableDefinitionEntry")) {
    CHECK(t.down_first_child());
    // Name
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    if (t.right()) {
      // Initialization
      s->append(" = ");
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    }
    t.up();
    return;
  }

  if (type == ss->findString("ConstructorDeclaration") || type == ss->findString("MethodDeclaration")) {
    indent(s, depth);
    int last_child = t.node().last_child;
    CHECK(t.down_first_child());
    // Modifiers
    size_t last_pos = s->size() - 1;
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    bool is_abstract = s->find("abstract", last_pos) != std::string::npos;
    bool is_interface = false;
    {
      ConstLocalTreeTraversal tmp = t;
      while (tmp.up()) {
        if (tmp.node().type == ss->findString("ClassDeclaration")) {
          break;
        }
        if (tmp.node().type == ss->findString("InterfaceDeclaration")) {
          is_interface = true;
          break;
        }
      }
    }
    CHECK(t.right());
    if (type == ss->findString("MethodDeclaration")){
      // Return value;
      s->append(" ");
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      CHECK(t.right());
    }
    // Name
    s->append(" ");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append("(");
    // Parameters
    CHECK(t.right() || is_abstract || is_interface);
    while (t.position() != last_child) {
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      CHECK(t.right());
      if (t.position() != last_child) {
        s->append(", ");
      }
    }
    if (is_abstract) {
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      s->append(");\n");
    } else {
      // Body
      s->append(") ");
      PrettyPrintTraverseJava(s, t, ss, depth + 1, highlighted_position, is_highlighting);
    }

    t.up();
    return;
  }

  if (type == ss->findString("Select")){
    CHECK(t.down_first_child());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(t.right());
    s->append(".");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("Throw")) {
    indent(s, depth);
    s->append("throw ");
    CHECK(t.down_first_child());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    s->append(";\n");
    t.up();
    return;
  }

  if (type == ss->findString("This")) {
    s->append("this");
    return;
  }

  if (type == ss->findString("Super")) {
    s->append("super");
    return;
  }

  if (type == ss->findString("Break")) {
    indent(s, depth);
    s->append("break");
    if (t.down_first_child()){
      s->append(" ");
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      CHECK(!t.right());
      t.up();
    }
    s->append(";\n");

    return;
  }

  if (type == ss->findString("Continue")) {
    indent(s, depth);
    s->append("continue;\n");
    return;
  }

  if (type == ss->findString("ConstructorInvocation")) {
    int last_child = t.node().last_child;
    CHECK(t.down_first_child());
    // Class Name
    s->append("new ");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append("(");
    // Parameters
    while (t.right()) {
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      if (t.position() != last_child) {
        s->append(", ");
      }
    }
    s->append(")");
    t.up();
    return;
  }


  if (type == ss->findString("MethodInvocation")) {
    int last_child = t.node().last_child;
    CHECK(t.down_first_child());
    // Target
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(t.right());
    // Name
    s->append(".");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append("(");
    // Parameters
    while (t.right()) {
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      if (t.position() != last_child) {
        s->append(", ");
      }
    }
    s->append(")");
    t.up();
    return;
  }

  if (type == ss->findString("InstanceOf")) {
    CHECK(t.down_first_child());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(t.right());
    s->append(" instanceof ");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("BinaryExpression")) {
    int value = t.node().value;
    CHECK(t.down_first_child());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(t.right());
    s->append(" ");
    s->append(ss->getString(value));
    s->append(" ");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("UnaryExpression")) {
    std::string value = std::string(ss->getString(t.node().value));
    CHECK(t.down_first_child());
    if (value.at(0) == '?') {
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      s->append(value.substr(1));
    } else {
      s->append(value);
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    }

    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("Synchronized")) {
    indent(s, depth);
    CHECK(t.down_first_child());
    // Guard
    s->append("synchronized (");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") ");
    CHECK(t.right());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("If")) {
    indent(s, depth);
    CHECK(t.down_first_child());
    // Guard
    s->append("if (");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") ");
    CHECK(t.right());
    // If branch
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    if (t.right()) {
      // Else branch
      s->pop_back();
      s->append(" else ");
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    }
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("DoWhile")) {
    indent(s, depth);
    CHECK(t.down_first_child());
    // Body
    s->append("do ");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);

    CHECK(t.right());
    s->pop_back();
    s->append(" while (");
    // guard
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(");\n");
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("While")) {
    indent(s, depth);
    CHECK(t.down_first_child());
    // Guard
    s->append("while (");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") ");
    CHECK(t.right());
    // body
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  //@TODO unable to distinguish different parts of for
  if (type == ss->findString("For")) {

    indent(s, depth);
    s->append("for (");
    CHECK(t.down_first_child());
    do {
      if (t.node().type == ss->findString("Block")) {
        s->append(") ");
        PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
        CHECK(!t.right());
      } else {
        PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
        s->append(";");
      }
    } while (t.right());
    t.up();
    return;
  }

  if (type == ss->findString("ForEach")) {
    indent(s, depth);
    s->append("for (");
    CHECK(t.down_first_child());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(" : ");
    CHECK(t.right());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") ");
    CHECK(t.right());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  if (t.node().type == ss->findString("LabelledStatement")){
    CHECK(t.down_first_child());
    indent(s, depth - 1);
    //Label
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(":\n");
    CHECK(t.right());
    //Statement
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("Switch")) {
    indent(s, depth);
    CHECK(t.down_first_child());
    // Guard
    s->append("switch (");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") ");
    CHECK(t.right());
    // block
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("Case")) {
    indent(s, depth);
    CHECK(t.down_first_child());
    s->append("case ");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(": \n");
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("Default")) {
    CHECK(!t.down_first_child());
    s->append("default:\n");
    return;
  }

  if (type == ss->findString("Return")) {
    indent(s, depth);
    if (t.down_first_child()) {
      s->append("return ");
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      s->append(";\n");
      t.up();
    } else {
      s->append("return;\n");
    }
    return;
  }

  if (type == ss->findString("Cast")) {
    CHECK(t.down_first_child());
    s->append("(");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") ");
    CHECK(t.right());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("InlineIfExpression")) {
    CHECK(t.down_first_child());
    // Guard
    s->append("(");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") ? ");
    CHECK(t.right());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(" : ");
    CHECK(t.right());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("Try")) {
    indent(s, depth);
    CHECK(t.down_first_child());
    // Try
    s->append("try ");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);

    while (t.right()) {
      if (t.node().type == ss->findString("Catch")) {
        PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      } else {
        // Finally
        CHECK(!t.right());
        s->pop_back();
        s->append(" finally ");
        PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      }
    }
    t.up();
    return;
  }

  if (type == ss->findString("Catch")) {
    CHECK(t.down_first_child());
    s->pop_back();
    s->append(" catch(");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(t.right());
    s->append(") ");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("ArrayAccess")) {
    CHECK(t.down_first_child());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(t.right());
    s->append("[");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append("]");
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("ArrayCreation")) {
    CHECK(t.down_first_child());
    s->append("new ");
    // Type
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    while(t.right()) {
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    }
    t.up();
    return;
  }

  if (type == ss->findString("ArrayDimension")) {
    s->append("[");
    if(t.down_first_child()) {
      PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      CHECK(!t.right());
      t.up();
    }
    s->append("]");
    return;
  }

  if (type == ss->findString("ArrayInitializer")) {
    s->append("{");
    if (t.down_first_child()) {
      while(t.right()) {
        PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
      }
      t.up();
    }
    s->append("}");
    return;
  }

  if (type == ss->findString("ClassLiteral")) {
    CHECK(t.down_first_child());
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(".class");
    CHECK(!t.right());
    t.up();
    return;
  }

  if (type == ss->findString("Assert")) {
    CHECK(t.down_first_child());
    s->append("Assert(");
    PrettyPrintTraverseJava(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(");");
    CHECK(!t.right());
    t.up();
    return;
  }

  LOG(FATAL) << "Unhandled type: '" << ss->getString(type) << "'";
}

void TreeStorage::PrettyPrintTraverseJS(std::string* s, ConstLocalTreeTraversal& t, const StringSet* ss, int depth, int highlighted_position, bool is_highlighting) const {

  std::string type = ss->getString(t.node().type);

  // Workaround for highlighting such that we dont need to rewrite the code below
  if (t.position() == highlighted_position && !is_highlighting){
    s->append(std::string(HighlightColors::GREEN));
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, true);
    s->append(std::string(HighlightColors::DEFAULT));
    return;
  }


  if (t.node().type == ss->findString("FunctionDeclaration") || t.node().type == ss->findString("FunctionExpression")) {
    int last_child_pos = t.node().last_child;
    s->append("function");

    int parent_type = t.node().type;
    CHECK(t.down_first_child());
    // If the function is not anonymous first child is the name identifier
    if (parent_type == ss->findString("FunctionDeclaration")){
      // disable check to support old AST version
      CHECK(t.node().type == ss->findString("Identifier")) << "Expected function name";
      if (t.node().type == ss->findString("Identifier")) {
        s->append(" ");
        PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
        CHECK(t.right());
      }
    }

    bool first = true;
    s->append("(");
    // Subsequent children (except the last) are parameters
    while (t.position() != last_child_pos){
      if (!first) {
        appendCommaFormatted(s);
        if (s->back() == '\n') {
          indent(s, depth + 1);
        }
      }
      first = false;
      PrettyPrintTraverseJS(s, t, ss, depth + 1, highlighted_position, is_highlighting);
      t.right();
    }
    s->append(") ");

    //Last child is function body
    CHECK(t.node().type == ss->findString("BlockStatement")) << "Expected to have single BlockStatement";
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("BlockStatement") || t.node().type == ss->findString("Program")) {
    if (t.node().type == ss->findString("BlockStatement")) {
//      s->append("\n");
//      indent(s, depth);
      s->append(" {\n");
      ++depth;
    }
    if (t.down_first_child()){
      do {
        indent(s, depth);
        PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
        if (t.node().type != ss->findString("TryStatement") &&
            t.node().type != ss->findString("IfStatement") &&
            t.node().type != ss->findString("ForInStatement") &&
            t.node().type != ss->findString("ForStatement") &&
            t.node().type != ss->findString("DoWhileStatement") &&
            t.node().type != ss->findString("WhileStatement")) {
          s->append(";\n");
        }
      } while (t.right());
      t.up();
    }

    if (t.node().type == ss->findString("BlockStatement")) {
      --depth;
      indent(s, depth);
      s->append("}\n");
    }
    return;
  }

  if (t.node().type == ss->findString("VariableDeclaration")) {
    s->append("var ");
    if (t.down_first_child()){
      do {
        PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
        if (!t.right())
          break;
        s->append(", ");
      } while (true);
      t.up();
    }

    return;
  }

  if (t.node().type == ss->findString("VariableDeclarator")) {
    s->append(ss->getString(t.node().value));
    if (t.down_first_child()){
      s->append(" = ");
      do {
        PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
      } while (t.right());
      t.up();
    }

    return;
  }

  if (t.node().type == ss->findString("CallExpression") || t.node().type == ss->findString("NewExpression")) {
    if (t.node().type == ss->findString("NewExpression")) s->append("new ");
    if (!t.down_first_child()) return;
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append("(");
    bool first = true;
    while (t.right()) {
      if (!first) appendCommaFormatted(s);
      if (s->back() == '\n') {
        indent(s, depth + 1);
      }
      first = false;
      PrettyPrintTraverseJS(s, t, ss, depth + 1, highlighted_position, is_highlighting);
    }
    if (s->back() == '\n') {
      indent(s, depth);
    }
    s->append(")");
    t.up();
    return;
  }


  if (t.node().type == ss->findString("AssignmentExpression")) {
    if (t.tree_storage()->NumNodeChildren(t.position()) != 2) return;
    CHECK(t.down_first_child());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(" = ");
    CHECK(t.right());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(!t.right());
    t.up();
    return;
  }

  if (t.node().type == ss->findString("ObjectExpression")) {
//    s->append("\n");
//    indent(s, depth);
    s->append(" {\n");
    ++depth;
    if (t.down_first_child()) {
      bool first = true;
      do {
        if (!first) {
          appendCommaFormatted(s);
          if (s->back() != '\n') {
            s->append("\n");
          }
        }
        first = false;
        indent(s, depth);
        PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
        s->append(":");
        CHECK(t.down_first_child());
        PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
        t.up();
      } while (t.right());
      t.up();
    }
    --depth;
    if (s->back() != '\n') {
//      s->append("\n");
      indent(s, depth);
      s->append("}\n");
    } else {
      s->back() = '}';
      s->append(" ");
    }


    return;
  }

  if (t.node().type == ss->findString("UnaryExpression")) {
//    if (t.tree_storage()->NumNodeChildren(t.position()) != 1) return;
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 1) << "Expected 1 child for '" << ss->getString(t.node().type) << "'";
    s->append(ss->getString(t.node().value));
    s->append(" ");
    CHECK(t.down_first_child());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("BinaryExpression")) {
//    if (t.tree_storage()->NumNodeChildren(t.position()) != 2) return;
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 2) << "Expected 2 children for '" << ss->getString(t.node().type) << "'";
    CHECK(t.down_first_child());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(" ");
    t.up();
    s->append(ss->getString(t.node().value));
    s->append(" ");
    CHECK(t.down_last_child());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("MemberExpression")) {
//    if (t.tree_storage()->NumNodeChildren(t.position()) != 2) return;
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 2) << "Expected 2 children for '" << ss->getString(t.node().type) << "'";
    CHECK(t.down_first_child());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(".");
    CHECK(t.right());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("Identifier") || t.node().type == ss->findString("Property")) {
    if (t.node().value != -1)
      s->append(ss->getString(t.node().value));
    return;
  }

  // Old parser version
  if (t.node().type == ss->findString("Literal")) {
    if (t.node().value == -1) {
      s->append("?number");
    } else {
      bool is_null = (strcmp(ss->getString(t.node().value), "null") == 0);
      if (!is_null){
        s->append("'");
      }
      s->append(ss->getString(t.node().value));
      if (!is_null){
        s->append("'");
      }
    }
    return;
  }

  if (t.node().type == ss->findString("ThisExpression")) {
      s->append("this");
      return;
  }

  if (t.node().type == ss->findString("LiteralNull")) {
    s->append("null");
    return;
  }

  if (t.node().type == ss->findString("LiteralBoolean") ||
      t.node().type == ss->findString("LiteralRegExp") ||
      t.node().type == ss->findString("LiteralNumber")) {
    CHECK(t.node().value != -1);
    s->append(ss->getString(t.node().value));
    return;
  }

  if (t.node().type == ss->findString("LiteralString")) {
    CHECK(t.node().value != -1);
    s->append("\"");
    s->append(ss->getString(t.node().value));
    s->append("\"");
    return;
  }

  if (t.node().type == ss->findString("ArrayExpression")){
    s->append("[");
    bool first = true;
    if (t.down_first_child()) {
      do {
        if (!first) s->append(", ");
        first = false;
        PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
      } while (t.right());
      t.up();
    }
    s->append("]");
    return;
  }

  if (t.node().type == ss->findString("ArrayAccess")){
    if (t.tree_storage()->NumNodeChildren(t.position()) != 2) return;
    CHECK(t.down_first_child());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append("[");
    CHECK(t.right());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append("]");
    t.up();
    return;
  }

  if (t.node().type == ss->findString("ExpressionStatement")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 1) << "Expected single child for '" << ss->getString(t.node().type) << "'";
    CHECK(t.down_first_child());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("UpdateExpression")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 1) << "Expected single child for '" << ss->getString(t.node().type) << "'";
    // prefix value: ?++
    // postfix value: ++?
    CHECK(t.node().value >= 0);
    const char* value = ss->getString(t.node().value);
    CHECK(strlen(value) == 3);
    CHECK(t.down_first_child());
    if (value[0] == '?') {
      PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
      // skip the '?' character
      s->append((value + 1));
    } else {
      s->append(value, 2);
      PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    }
    t.up();
    return;
  }

  if (t.node().type == ss->findString("EmptyStatement")){
    //Included such that we know how to handle the ForStatement in which some of the init/test/update can be null.
    return;
  }

  if (t.node().type == ss->findString("ForInStatement")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 3) << "Expected 3 childs for '" << ss->getString(t.node().type) << "'";
    CHECK(t.down_first_child());
    s->append("for (");
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(" in ");
    CHECK(t.right());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") ");
    CHECK(t.right());
    PrettyPrintTraverseJS(s, t, ss, depth + 1, highlighted_position, is_highlighting);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("ForStatement")){
    if (t.tree_storage()->NumNodeChildren(t.position()) != 4) {
      LOG(INFO) << "Don't know how to output 'ForStatement'. Please use newer version of tern parser.";
      return;
    }
    CHECK(t.down_first_child());
    s->append("for (");
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append("; ");
    CHECK(t.right());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append("; ");
    CHECK(t.right());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") ");
    CHECK(t.right());
    PrettyPrintTraverseJS(s, t, ss, depth + 1, highlighted_position, is_highlighting);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("SequenceExpression")){
    if (t.down_first_child()){
      do {
        PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
        if (!t.right())
          break;
        s->append(", ");
      } while (true);
      t.up();
    }
    return;
  }

  if (t.node().type == ss->findString("ConditionalExpression")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 3) << "Expected 3 childs for '" << ss->getString(t.node().type) << "'";
    CHECK(t.down_first_child());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(t.right());
    s->append(" ? ");
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(t.right());
    s->append(" : ");
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("IfStatement")){
    CHECK(t.down_first_child());
    s->append("if (");
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") ");
    int id = t.position();
    CHECK(t.right()) << "id: " << id << ", " << t.node().first_child << " - " << t.node().last_child;
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    if (t.right()){
      indent(s, depth);
      s->append(" else ");
      PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    }
    t.up();
    return;
  }

  if (t.node().type == ss->findString("LogicalExpression")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 2) << "Expected 2 childs for '" << ss->getString(t.node().type) << "'";
    CHECK(t.down_first_child());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    t.up();
    CHECK(t.node().value >= 0);
    StringAppendF(s, " %s ", ss->getString(t.node().value));
    CHECK(t.down_last_child());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("WhileStatement")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 2) << "Expected 2 childs for '" << ss->getString(t.node().type) << "'";
    CHECK(t.down_first_child());
    s->append("while (");
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") ");
    CHECK(t.right());
    PrettyPrintTraverseJS(s, t, ss, depth + 1, highlighted_position, is_highlighting);
    t.up();
    return;
  }


  if (t.node().type == ss->findString("DoWhileStatement")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 2) << "Expected 2 childs for '" << ss->getString(t.node().type) << "'";
    CHECK(t.down_last_child());
    s->append("do ");
    PrettyPrintTraverseJS(s, t, ss, depth + 1, highlighted_position, is_highlighting);
    s->append(" while (");
    t.up();
    CHECK(t.down_first_child());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(")");
    t.up();
    return;
  }

  if (t.node().type == ss->findString("ReturnStatement")){
    s->append("return ");
    if (t.down_last_child()) {
      PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
      CHECK(!t.right());
      t.up();
    }

    return;
  }

  if (t.node().type == ss->findString("LabeledStatement")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 1) << "Expected 1 child for '" << ss->getString(t.node().type) << "'";
    CHECK(t.node().value != -1);
    StringAppendF(s, "%s:\n", ss->getString(t.node().value));
    CHECK(t.down_first_child());

    // body
    PrettyPrintTraverseJS(s, t, ss, depth + 1, highlighted_position, is_highlighting);

    t.up();
    return;
  }

  if (t.node().type == ss->findString("CatchClause")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 2) << "Expected 2 children for '" << ss->getString(t.node().type) << "'";
    CHECK(t.down_first_child());

    indent(s, depth);
    s->append("catch (");
    // argument
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(")");
    // body
    CHECK(t.right());
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("ThrowStatement")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 1) << "Expected single child for '" << ss->getString(t.node().type) << "'";
    CHECK(t.down_first_child());
    s->append("throw ");
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("SwitchStatement")){
    CHECK(t.down_first_child());
    s->append("switch(");
    // test expressions
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    s->append(") {\n");
    while (t.right()){
      PrettyPrintTraverseJS(s, t, ss, depth + 1, highlighted_position, is_highlighting);
    }
    t.up();
    s->append("}\n");
    return;
  }

  if (t.node().type == ss->findString("SwitchCase")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 2) << "Expected 2 children for '" << ss->getString(t.node().type) << "'";
    indent(s, depth);

    CHECK(t.down_first_child());
    if (t.node().type == ss->findString("EmptyStatement")){
      // default branch
      s->append("default:\n");
    } else {
      // switch case test
      s->append("case ");
      PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
      s->append(":\n");
    }


    CHECK(t.right());
    // We expect body to be wrapped in the BlockStatement.
    CHECK(t.node().type == ss->findString("BlockStatement"));
    std::string tmp_s;
    PrettyPrintTraverseJS(&tmp_s, t, ss, depth + 1, highlighted_position, is_highlighting);

    // Remove the curly braces of the BlockStatement which was added by the parser
    tmp_s.erase(tmp_s.find_first_of('{'),1);
    tmp_s.erase(tmp_s.find_last_of('}'),1);
    s->append(tmp_s);
    t.up();
    return;
  }

  if (t.node().type == ss->findString("TryStatement")){
    CHECK(t.tree_storage()->NumNodeChildren(t.position()) == 3) << "Expected 3 children for '" << ss->getString(t.node().type) << "'";
    CHECK(t.down_first_child());
    s->append("try");
    // body
    PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    CHECK(t.right());
    // optional catch clause, if not present then EmptyStatement
    if (t.node().type != ss->findString("EmptyStatement")){
      PrettyPrintTraverseJS(s, t, ss, depth, highlighted_position, is_highlighting);
    }
    // optional finally clause, if not present then EmptyStatement
    CHECK(t.right());
    if (t.node().type != ss->findString("EmptyStatement")){
      s->append("finally ");
      PrettyPrintTraverseJS(s, t, ss, depth + 1, highlighted_position, is_highlighting);
    }
    t.up();
    return;
  }

  if (t.node().type == ss->findString("ContinueStatement")){
    CHECK(!t.down_first_child());
    s->append("continue");
    // is it a labeled continue?
    if (t.node().value != -1) {
      s->append(" ");
      s->append(ss->getString(t.node().value));
    }
    return;
  }

  if (t.node().type == ss->findString("BreakStatement")){
    CHECK(!t.down_first_child());
    s->append("break");
    // is it a labeled break?
    if (t.node().value != -1) {
      s->append(" ");
      s->append(ss->getString(t.node().value));
    }
    return;
  }

  //TODO: AssignmentPattern

  LOG(FATAL) << "Unhandled type: '" << ss->getString(t.node().type) << "'";
}


std::string TreeStorage::DebugStringAsSource(const StringSet* ss, int highlighted_position) const {
  CHECK(ss != nullptr) << "Unable to PrettyPrint code without the StringSet";

  std::string result;
  ConstLocalTreeTraversal t(this, 0);
  PrettyPrintTraverse(&result, t, ss, 0, highlighted_position, false);
  return result;
}

std::string TreeStorage::DebugStringAsSourceWindow(const StringSet* ss, int highlighted_position, int window_size) const {
  CHECK(ss != nullptr) << "Unable to PrettyPrint code without the StringSet";

  std::string result;
  ConstLocalTreeTraversal t(this, 0);
  PrettyPrintTraverse(&result, t, ss, 0, highlighted_position, false);

  if (result.find(HighlightColors::GREEN) != std::string::npos && result.find(HighlightColors::DEFAULT) != std::string::npos){

    // Skip window_size lines backward from beginning of highlighted region
    std::size_t begin_idx = result.find(HighlightColors::GREEN);
    int lines_skipped = 0;
    while (lines_skipped < window_size && begin_idx != std::string::npos) {
      begin_idx = result.find_last_of('\n', begin_idx - 1);
      lines_skipped++;
    }

    // Skip window_size lines forward from end of highlighted region
    std::size_t end_idx = result.find(HighlightColors::DEFAULT);
    lines_skipped = 0;
    while (lines_skipped < window_size && end_idx != std::string::npos) {
      end_idx = result.find_first_of('\n', end_idx + 1);
      lines_skipped++;
    }

    if (begin_idx == std::string::npos) {
      begin_idx = 0;
    }
    if (end_idx == std::string::npos){
      end_idx = result.size();
    }
    result = result.substr(begin_idx, end_idx - begin_idx);
  }

  return result;
}

std::string TreeStorage::NodeToString(const StringSet* ss, int node) const {
  std::string result;
  CHECK( node < (int) nodes_.size() && node >= 0) << "number of nodes: " << nodes_.size() << ", position: " << node;
  if (ss == nullptr || nodes_[node].type < 0) {
    StringAppendF(&result, "%d", nodes_[node].type);
  } else {
    StringAppendF(&result, "%s", ss->getString(nodes_[node].type));
  }

  if (ss == nullptr || nodes_[node].value < 0) {
    if (nodes_[node].value != -1) {
      StringAppendF(&result, ":%d", nodes_[node].value);
    }
  } else {
    StringAppendF(&result, ":%s", ss->getString(nodes_[node].value));
  }
  return result;
}

void TreeStorage::Canonicalize() {
  TreeStorage tmp(parent_, position_in_parent_);
  tmp.nodes_.reserve(nodes_.size());
  auto write_it = LocalEpsTreeTraversal(&tmp, 0).begin();
  for (auto it = ConstLocalEpsTreeTraversal(this, 0).begin(); !it.at_end(); ++it) {
    CHECK(!write_it.at_end());
    write_it->type = it->type;
    write_it->value = it->value;
    ++write_it;
  }
  CHECK(write_it.at_end());
  swap(tmp);
}

void TreeStorage::GetSubtreesOfMaxSize(int max_size, std::vector<int>* subtrees) {
  std::vector<int> tree_sizes;
  GetTreeSizesAtNodes(&tree_sizes);
  for (unsigned i = 0; i < nodes_.size(); ++i) {
    // do not include subtrees of already included trees
    if (tree_sizes[i] == 0) {
      if (nodes_[i].first_child >= 0) {
        tree_sizes[nodes_[i].first_child] = 0;
      }
      if (nodes_[i].right_sib >= 0) {
        tree_sizes[nodes_[i].right_sib] = 0;
      }
    } else {
      if (tree_sizes[i] <= max_size) {
        subtrees->push_back(i);
        if (nodes_[i].first_child >= 0) {
          tree_sizes[nodes_[i].first_child] = 0;
        }
      }
    }
  }
}

void TreeStorage::GetTreeSizesAtNodes(std::vector<int>* tree_sizes) {
  Canonicalize();
  tree_sizes->assign(nodes_.size(), 1);

  for (unsigned i = nodes_.size(); i > 0;) {
    --i;
    if (nodes_[i].parent < 0) continue;
    (*tree_sizes)[nodes_[i].parent] += (*tree_sizes)[i];
  }
}

int TreeStorage::CheckNodeConsistencyRecursive(int node_id, int max_depth) const {
  CHECK_GE(max_depth, 0) << "Max depth reached.\n" << DebugString();

  const TreeNode& node = nodes_[node_id];
  CHECK_NE(node.parent, TREEPOINTER_DEALLOCATED) << DebugString();
  if (node.parent >= 0 && node.left_sib < 0) {
    CHECK_EQ(nodes_[node.parent].first_child, node_id) << DebugString();
  }
  if (node.left_sib < 0) {
    if (node_id == 0 && parent_ != nullptr) {
      CHECK_EQ(node.child_index, parent_->nodes_[position_in_parent_].child_index) << DebugString();
    } else {
      CHECK_EQ(node.child_index, 0) << DebugString();
    }
  }
  if (node.left_sib >= 0) {
    CHECK_EQ(nodes_[node.left_sib].right_sib, node_id);
    CHECK_EQ(nodes_[node.left_sib].parent, node.parent) << DebugString();
    CHECK_EQ(nodes_[node.left_sib].child_index + 1, node.child_index) << DebugString();
  }
  if (node.right_sib >= 0) {
    CHECK_EQ(nodes_[node.right_sib].left_sib, node_id) << DebugString();
  }
  if (node.parent >= 0 && node.right_sib < 0) {
    CHECK_EQ(nodes_[node.parent].last_child, node_id) << DebugString();
  }
  if (node.first_child >= 0) {
    CHECK_EQ(nodes_[node.first_child].parent, node_id) << DebugString();
  }
  if (node.last_child >= 0) {
    CHECK_EQ(nodes_[node.last_child].parent, node_id) << DebugString();
  }

  int result = 1;
  if (node.first_child >= 0) result += CheckNodeConsistencyRecursive(node.first_child, max_depth - 1);
  if (node.right_sib >= 0) result += CheckNodeConsistencyRecursive(node.right_sib, max_depth - 1);
  return result;
}

void TreeStorage::CheckConsistency() const {
  unsigned num_nodes = CheckNodeConsistencyRecursive(0, 32);
  unsigned num_freed_nodes = 0;
  int dealloc = first_free_node_;
  while (dealloc != -1) {
    dealloc = nodes_[dealloc].type;
    ++num_freed_nodes;
    if (num_freed_nodes > nodes_.size()) LOG(FATAL) << "Cycle in freed nodes. " << DebugString();
  }
  CHECK_EQ(num_nodes + num_freed_nodes, nodes_.size()) << DebugString();
}

void TreeStorage::RemoveNodeChildren(int start_node_id) {
  int curr = nodes_[start_node_id].first_child;
  if (curr < 0) return;

  TreeIterator<TreeNode, LocalTreeTraversal> it(
      LocalTreeTraversal(this, curr), TreeIteratorMode::POST_ORDER_FORWARD_DFS);
  nodes_[start_node_id].first_child = -1;
  nodes_[start_node_id].last_child = -1;
  while (it.position() != start_node_id) {
    int node_to_delete = it.position();
    ++it;
    DeallocateNode(node_to_delete);
  }
}

void TreeStorage::SubstituteNode(int node_id, const TreeSubstitution& subst) {
  DCHECK(CanSubstituteNode(node_id, subst));
  if (subst.data.size() == 0) {
    RemoveNode(node_id);
    return;
  }
  RemoveNodeChildren(node_id);

  std::queue<std::pair<int, int> > rqueue;
  rqueue.push(std::pair<int, int>(0, node_id));
  while (!rqueue.empty()) {
    std::pair<int, int> el = rqueue.front();
    rqueue.pop();

    const TreeSubstitution::Node& n = subst.data[el.first];
    CHECK_NE(n.type, TreeNode::EMPTY_NODE_LABEL);
    nodes_[el.second].type = n.type;
    nodes_[el.second].value = n.value;

    if (n.first_child != -1) {
      LocalEpsTreeTraversal it(this, el.second);
      CHECK(it.down_first_child());
      it.node().type = TreeNode::UNKNOWN_LABEL;
      it.node().value = TreeNode::UNKNOWN_LABEL;
      it.write_node();
      CHECK_NE(it.position(), -1);
      if (n.first_child >= 0) {
        rqueue.push(std::pair<int, int>(n.first_child, it.position()));
      }
    }
    if (n.right_sib != -1) {
      if (n.right_sib == -2 && el.second == 0) continue;

      LocalEpsTreeTraversal it(this, el.second);
      CHECK(it.right());
      if (it.node().type == TreeNode::EMPTY_NODE_LABEL) {
        it.node().type = TreeNode::UNKNOWN_LABEL;
        it.node().value = TreeNode::UNKNOWN_LABEL;
      }
      it.write_node();
      CHECK_NE(it.position(), -1);
      if (n.right_sib >= 0) {
        rqueue.push(std::pair<int, int>(n.right_sib, it.position()));
      }
    }
  }
}

void TreeStorage::SubstituteSingleNode(int node_id, const TreeSubstitution::Node& node) {
  RemoveNodeChildren(node_id);
  nodes_[node_id].type = node.type;
  nodes_[node_id].value = node.value;
  if (node.first_child != -1) {
    LocalEpsTreeTraversal it(this, node_id);
    CHECK(it.down_first_child());
    it.node().type = TreeNode::UNKNOWN_LABEL;
    it.node().value = TreeNode::UNKNOWN_LABEL;
    it.write_node();
  }
  if (node.right_sib != -1 && node_id != 0) {
    LocalEpsTreeTraversal it(this, node_id);
    CHECK(it.right());
    it.node().type = TreeNode::UNKNOWN_LABEL;
    it.node().value = TreeNode::UNKNOWN_LABEL;
    it.write_node();
  }
}

void TreeStorage::SubstituteNodeType(int node_id, int type) {
  if (type == TreeNode::EMPTY_NODE_LABEL) {
    RemoveNode(node_id);
  } else {
    RemoveNodeChildren(node_id);
    nodes_[node_id].type = type;
    nodes_[node_id].value = TreeNode::UNKNOWN_LABEL;
    {
      LocalEpsTreeTraversal it(this, node_id);
      CHECK(it.down_first_child());
      it.node().type = TreeNode::UNKNOWN_LABEL;
      it.node().value = TreeNode::UNKNOWN_LABEL;
      it.write_node();
    }
    if (node_id != 0) {
      LocalEpsTreeTraversal it(this, node_id);
      CHECK(it.right());
      it.node().type = TreeNode::UNKNOWN_LABEL;
      it.node().value = TreeNode::UNKNOWN_LABEL;
      it.write_node();
    }
  }
}

void TreeStorage::InlineIntoParent(TreeStorage* parent) {
  CHECK_EQ(parent_, parent);
  if (parent == nullptr) return;
  parent->RemoveNodeChildren(position_in_parent_);
  LocalEpsTreeTraversal writer(parent, position_in_parent_);
  ConstLocalEpsTreeTraversal reader(this, 0);
  auto writer_it = writer.begin();
  for (auto it = reader.begin(); !it.at_end(); ++it) {
    CHECK(!writer_it.at_end());
    writer_it->type = it->type;
    writer_it->value = it->value;
    ++writer_it;
  }
}

int TreeStorage::NumNodeChildren(int position) const {
  ConstLocalTreeTraversal t(this, position);
  if (!t.down_first_child()) return 0;

  int num_children = 1;
  while (t.right()){
    num_children++;
  }
  return num_children;
}

void TreeStorage::Parse(const Json::Value& v, StringSet* ss) {
  parent_ = nullptr;
  position_in_parent_ = -1;
  first_free_node_ = -1;

  CHECK(v.isArray());

  int node_count = v.size();
  while (node_count > 0 && !v[node_count - 1].isObject()) {
    --node_count;
  }

  TreeNode empty_node( TreeNode::UNKNOWN_LABEL, -1, -1, -1, -1, -1, -1, -1 );
  nodes_.assign(node_count, empty_node);
  nodes_[0].child_index = 0;

  for (int node_id = 0; node_id < node_count; ++node_id) {
    const Json::Value& json_node = v[node_id];
    CHECK(!json_node.isNull());
    CHECK(json_node.isObject());
    if (json_node.isMember("id")) {
      CHECK_EQ(node_id, json_node["id"].asInt());
    }
    {
      // Parse field type.
      const Json::Value& type = json_node["type"];
      CHECK(type.isString());
      nodes_[node_id].type = ss->addString(type.asCString());
    }
    if (json_node.isMember("value")) {
      // Parse field value.
      const Json::Value& value = json_node["value"];
      nodes_[node_id].value = value.isString() ? ss->addString(value.asCString()) : -1;
    } else {
      nodes_[node_id].value = -1;
    }
    {
      // Add children.
      const Json::Value& children = json_node["children"];
      if (children.isArray()) {
        int last_child_id = -1;
        for (Json::ArrayIndex i = 0; i < children.size(); ++i) {
          int child_node_id = children[i].asInt();
          // A child always must have a node id higher than its parent. This is to ensure we have a tree.
          CHECK_GE(child_node_id, node_id);
          CHECK_LT(child_node_id, node_count);
          nodes_[child_node_id].child_index = i;
          nodes_[child_node_id].parent = node_id;
          if (last_child_id == -1) {
            nodes_[node_id].first_child = child_node_id;
          } else {
            nodes_[last_child_id].right_sib = child_node_id;
            nodes_[child_node_id].left_sib = last_child_id;
          }
          nodes_[node_id].last_child = child_node_id;
          last_child_id = child_node_id;
        }
      }
    }
  }
}

TreeStorage TreeStorage::SubtreeFromNodeAsTree(int node) const {
  if (node == 0) return TreeStorage(*this);
  TreeStorage result(this, node);
  ConstLocalEpsTreeTraversal reader(this, node);
  LocalEpsTreeTraversal writer(&result, 0);
  ConstLocalEpsTreeTraversal reader_end(reader);
  CHECK(reader_end.right());
  auto write_it = writer.begin();
  auto it_end = reader_end.begin();
  for (auto it = reader.begin(); it != it_end; ++it) {
    CHECK(!write_it.at_end());
    write_it->type = it->type;
    write_it->value = it->value;
    ++write_it;
  }
  return result;
}

TreeStorage TreeStorage::GetSubtreeForCompletion(int position, bool is_for_node_type) const {
  int subtree_pos = node(position).parent;
  if (subtree_pos == -1) {
    subtree_pos = position;
  }

  TreeStorage subtree(this, subtree_pos);
  subtree.CheckConsistency();
  auto write_it = LocalEpsTreeTraversal(&subtree, 0).begin();
  for (auto it = ConstLocalEpsTreeTraversal(this, subtree_pos).begin(); !it.at_end(); ++it) {
    if (it.position() == position) {
      write_it->SetType(is_for_node_type ? TreeNode::UNKNOWN_LABEL : node(position).Type());
      write_it->SetValue(TreeNode::UNKNOWN_LABEL);
      ++write_it;
      break;
    } else {
      write_it->SetType(it->Type());
      write_it->SetValue(it->Value());
      ++write_it;
    }
  }

  return subtree;
}

bool TreeStorage::HasNonTerminal() const {
  ConstLocalTreeTraversal t(this, 0);
  for (auto it = t.begin(); !it.at_end(); ++it) {
    if (it->HasNonTerminal()) {
      return true;
    }
  }
  return false;
}

int TreeSize(ConstLocalTreeTraversal t) {
  int start_pos = t.position();
  int result = 0;
  for (;;) {
    ++result;
    if (t.down_first_child() == false) {
      // No children - leave the node.
      for (;;) {
        if (t.position() == start_pos) {
          return result;
        }
        // Leaving the node is first to the right, if not possible up until
        // a right sibling is present (or end of tree).
        if (t.right()) break;
        CHECK(t.up());
      }
    }
  }
}

void CompareTrees(ConstLocalTreeTraversal t1, ConstLocalTreeTraversal t2, TreeCompareInfo* info, bool only_types, int max_depth) {
  info->num_type_equalities = 0;
  info->num_type_diffs = 0;
  info->num_value_equalities = 0;
  info->num_value_diffs = 0;
  info->num_size_greater_diffs = 0;
  info->num_size_smaller_diffs = 0;
  int depth = 0;
  int start_t1 = t1.position();
  int start_t2 = t2.position();
  for (;;) {
    if (t1.node().type == t2.node().type) ++(info->num_type_equalities); else ++(info->num_type_diffs);
    if (!only_types) {
      if (t1.node().value == t2.node().value) ++(info->num_value_equalities); else ++(info->num_value_diffs);
    }

    if (depth < max_depth) {
      bool t1_down = t1.down_first_child();
      bool t2_down = t2.down_first_child();
      depth++;
      if (t1_down && t2_down) continue;
      if (t1_down && !t2_down) { do { info->num_size_greater_diffs += 2 * TreeSize(t1); } while (t1.right()); t1.up(); depth--; }
      if (!t1_down && t2_down) { do { info->num_size_smaller_diffs += 2 * TreeSize(t2); } while (t2.right()); t2.up(); depth--; }
    }

    for (;;) {
      if (t1.position() == start_t1) {
        CHECK(t2.position() == start_t2);
        return;
      }

      bool t1_right = t1.right();
      bool t2_right = t2.right();
      if (t1_right && t2_right) break;
      if (t1_right && !t2_right) { info->num_size_greater_diffs += 2 * TreeSize(t1); continue; }
      if (t2_right && !t1_right) { info->num_size_smaller_diffs += 2 * TreeSize(t1); continue; }

      CHECK(t1.up());
      CHECK(t2.up());
      depth--;
    }
  }

  CHECK(depth == 0);
}

void CompareTrees(ConstLocalTreeTraversal t1, ConstLocalTreeTraversal t2, int* num_equalities, int* num_diffs) {
  TreeCompareInfo info;
  CompareTrees(t1, t2, &info);
  *num_equalities = info.GetEqualities();
  *num_diffs = info.GetDifferences();
}

std::string PerTreeSizeTrainingStatistics::DebugString(const StringSet* ss, bool detailed) const {
  std::string result;

  StringAppendF(&result, "======= PerTreeSizeTrainingStatistics =======\n");
  result.append("size -> size of the reference tree\n");
  result.append("count -> number of completions of given size in the evaluation\n");
  result.append("Type -> success of predicting correct type of the node. These numbers are across all the predicted nodes, not just the root.\n");
  result.append("Size greater -> Size(predicted_tree) - Size(reference_tree) \n");
  result.append("Size smaller -> Size(reference_tree) - Size(predicted_tree) \n");
  if (detailed) {
    result += "\n";

    for (auto key_it = stats_per_predictor_.begin(); key_it != stats_per_predictor_.end(); ++key_it) {
      StringAppendF(&result, "%20s : [size][count] Type: correct/incorrect (prec),  Value: correct/incorrect (prec),  Size: greater/smaller\n", "");
      for (auto size_it = key_it->second.begin(); size_it != key_it->second.end(); ++size_it) {
        StringAppendF(&result, "%20s : [%4d][%5d] Type: %8d/%-8d (%4.f%%), Value: %8d/%-8d (%4.f%%), Size: %4d/%4d, Avg Node diff: (%4.2f)\n",
                    ss->getString(key_it->first),
                    size_it->first, size_it->second.num_aggregated_trees,
                    size_it->second.num_type_equalities, size_it->second.num_type_diffs, size_it->second.num_type_equalities*100.0 / static_cast<double>(size_it->second.num_type_equalities + size_it->second.num_type_diffs),
                    size_it->second.num_value_equalities, size_it->second.num_value_diffs, size_it->second.num_value_equalities*100.0 / static_cast<double>(size_it->second.num_value_equalities + size_it->second.num_value_diffs),
                    size_it->second.num_size_greater_diffs, size_it->second.num_size_smaller_diffs, size_it->second.AvgNodeDifference());
      }
      result += "\n";
    }
  }

  return result;
}

std::string TreeTrainingStatistics::DebugStringShort(const StringSet* ss, bool detailed) const {
  return StringPrintf("Type: %7d vs %7d labels (%.3f%%), Value: %7d vs %7d labels (%.3f%%), Size:  greater %7d, smaller   %7d",
      stats_.num_type_equalities, stats_.num_type_diffs, stats_.num_type_equalities*100.0 / static_cast<double>(stats_.num_type_equalities + stats_.num_type_diffs),
      stats_.num_value_equalities, stats_.num_value_diffs, stats_.num_value_equalities*100.0 / static_cast<double>(stats_.num_value_equalities + stats_.num_value_diffs),
      stats_.num_size_greater_diffs, stats_.num_size_smaller_diffs);
}

std::string TreeTrainingStatistics::DebugString(const StringSet* ss, bool detailed, std::string header) const {
  std::string result;

  StringAppendF(&result, "======= TreeTrainingStatistics =======\n");
  StringAppendF(&result, "%s\n", header.c_str());
  StringAppendF(&result, "Comparison Completion vs Reference\n");
  StringAppendF(&result, "Type:  correct %7d, incorrect %7d labels (precision %.3f%%)\n", stats_.num_type_equalities, stats_.num_type_diffs, stats_.num_type_equalities*100.0 / static_cast<double>(stats_.num_type_equalities + stats_.num_type_diffs));
  StringAppendF(&result, "Value: correct %7d, incorrect %7d labels (precision %.3f%%)\n", stats_.num_value_equalities, stats_.num_value_diffs, stats_.num_value_equalities*100.0 / static_cast<double>(stats_.num_value_equalities + stats_.num_value_diffs));
  StringAppendF(&result, "Size:  greater %7d, smaller   %7d\n", stats_.num_size_greater_diffs, stats_.num_size_smaller_diffs);
  if (detailed) {
    result += "\n";
    StringAppendF(&result, "%20s : Type: correct/incorrect (prec),  Value: correct/incorrect (prec),  Size: greater/smaller\n", "");

    std::vector<std::pair<int, int>> id_by_size;
    for (auto it = stats_per_predictor_.begin(); it != stats_per_predictor_.end(); ++it) {
      id_by_size.push_back(std::make_pair(it->second.GetDifferences() + it->second.GetEqualities(), it->first));
    }
    std::sort(id_by_size.begin(), id_by_size.end(), std::greater<std::pair<int,int>>());

    for (auto const& val : id_by_size) {
      const auto& it = stats_per_predictor_.find(val.second);
      CHECK(it != stats_per_predictor_.end());
//    for (auto it = stats_per_predictor_.begin(); it != stats_per_predictor_.end(); ++it) {
//      if (it->second.num_incorrect_labels != 0)
        StringAppendF(&result, "%20s : Type: %8d/%-8d (%4.f%%), Value: %8d/%-8d (%4.f%%), Size: %4d/%4d, Avg Node diff: (%4.2f)\n",
            (it->first == -1) ? "TYPE" : ss->getString(it->first),
                it->second.num_type_equalities, it->second.num_type_diffs, it->second.num_type_equalities*100.0 / static_cast<double>(it->second.num_type_equalities + it->second.num_type_diffs),
                it->second.num_value_equalities, it->second.num_value_diffs, it->second.num_value_equalities*100.0 / static_cast<double>(it->second.num_value_equalities + it->second.num_value_diffs),
                it->second.num_size_greater_diffs, it->second.num_size_smaller_diffs, it->second.AvgNodeDifference());
    }
  }

  return result;
}

void ParseTreesInFileWithParallelJSONParse(
    StringSet* ss,
    const char* filename,
    int start_offset,
    int num_records,
    bool show_progress,
    std::vector<TreeStorage>* trees) {

  static const int NUM_PARSING_THREADS = 8;

  std::unique_ptr<RecordInput> input(new FileRecordInput(filename));
  std::unique_ptr<InputRecordReader> reader(input->CreateReader());
  std::vector<std::thread> threads;
  int records = 0;
  std::mutex read_mutex;
  std::mutex parse_mutex;
  for (int thread_id = 0; thread_id < NUM_PARSING_THREADS; ++thread_id) {
    threads.push_back(std::thread([&](){
      std::string s;
      Json::Reader jsonreader;
      while (!reader->ReachedEnd()) {
        int pos = -1;
        {
          std::lock_guard<std::mutex> guard(read_mutex);
          reader->Read(&s);
          if (s.size() <= 2) continue;  // Skip empty lines.
          if (s == "[]") continue; // Skip empty jsons
          if (s[s.size() - 1] != ']') {
            printf("%s\n", s.c_str());
            continue;
          }
          ++records;
          if (records < start_offset)
            continue;
          if (records > start_offset + num_records)
            break;
          std::lock_guard<std::mutex> guard1(parse_mutex);
          pos = trees->size();
          trees->push_back(TreeStorage());
        }
        Json::Value v;
        if (!jsonreader.parse(s, v, false)) {
          if (s.size() > 128) {
            printf("%s\n", s.c_str() + s.size() - 128);
          } else {
            printf("%s\n", s.c_str());
          }
          LOG(FATAL)<< "Could not parse JSON in "
              << filename << ".\n Error: " << jsonreader.getFormattedErrorMessages();
        }

        {
          std::lock_guard<std::mutex> guard(parse_mutex);
          TreeStorage& tree = (*trees)[pos];
          tree.Parse(v, ss);
          if (show_progress && records % 16 == 0) {
            std::cerr << std::fixed << std::setprecision(2) << "\r processed files -> " << double(records - start_offset)/num_records*100 << "% [" << (records - start_offset) << "/" << num_records << "]";
          }
        }
      }
    }));
  }
  for (auto& thread : threads){
    thread.join();
  }
  LOG(INFO) << "Parsing done.";

  // Remove trees that are too large. For JavaScript, more than 30k corresponds to ~1% of files.
  trees->erase(
      std::remove_if(trees->begin(), trees->end(), [](const TreeStorage& tree) -> bool{
    return static_cast<int>(tree.NumAllocatedNodes()) > FLAGS_max_tree_size;
  }), trees->end());
  LOG(INFO) << "Remaining trees after removing trees with more than " << FLAGS_max_tree_size << " nodes: " << trees->size();
}

int EncodeTypeLabel(const TreeSubstitutionOnlyLabel& type_label) {
  CHECK_LE(type_label.type, 0x1fffffff) << "Too large type id.";
  unsigned x = type_label.type;
  x &= 0x3fffffffu;
  if (type_label.has_first_child) x |= 0x40000000u;
  if (type_label.has_right_sib) x |= 0x80000000u;
  return x;
}

TreeSubstitutionOnlyLabel DecodeTypeLabel(int encoded_label) {
  unsigned x = encoded_label;
  TreeSubstitutionOnlyLabel result;
  result.has_first_child = (x & 0x40000000u) != 0;
  result.has_right_sib = (x & 0x80000000u) != 0;
  x &= 0x3fffffffu;
  if (x & 0x20000000u) x |= 0xe0000000;
  result.type = x;
  return result;
}
