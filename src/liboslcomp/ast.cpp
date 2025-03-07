// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#include <algorithm>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#ifndef NDEBUG
#    include <atomic>
#endif

#include "osl_pvt.h"
#include "oslcomp_pvt.h"

#include <OpenImageIO/atomic.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/strutil.h>
namespace Strutil = OIIO::Strutil;

OSL_NAMESPACE_ENTER

namespace pvt {  // OSL::pvt


#ifndef NDEBUG
// When in DEBUG mode, track the number of AST nodes of each type that
// are allocated and remaining, and at program exit print a message about
// any leaked nodes.
namespace {
std::atomic<int> node_counts[ASTNode::_last_node];
std::atomic<int> node_counts_peak[ASTNode::_last_node];

class ScopeExit {
public:
    typedef std::function<void()> Task;
    explicit ScopeExit(Task&& task) : m_task(std::move(task)) {}
    ~ScopeExit() { m_task(); }

private:
    Task m_task;
};

ScopeExit print_node_counts([]() {
    for (int i = 0; i < ASTNode::_last_node; ++i)
        if (node_counts[i] > 0)
            Strutil::print("ASTNode type {:2}: {:5}   (peak {:5})\n", i,
                           node_counts[i], node_counts_peak[i]);
});
}  // namespace
#endif



ASTNode::ref
reverse(ASTNode::ref list)
{
    ASTNode::ref new_list;
    while (list) {
        ASTNode::ref next = list->next();
        list->m_next      = new_list;
        new_list          = list;
        list              = next;
    }
    return new_list;
}



ASTNode::ASTNode(NodeType nodetype, OSLCompilerImpl* compiler)
    : m_nodetype(nodetype)
    , m_compiler(compiler)
    , m_sourcefile(compiler->filename())
    , m_sourceline(compiler->lineno())
    , m_op(0)
    , m_is_lvalue(false)
{
#ifndef NDEBUG
    node_counts[nodetype] += 1;
    node_counts_peak[nodetype] += 1;
#endif
}



ASTNode::ASTNode(NodeType nodetype, OSLCompilerImpl* compiler, int op,
                 ASTNode* a)
    : m_nodetype(nodetype)
    , m_compiler(compiler)
    , m_sourcefile(compiler->filename())
    , m_sourceline(compiler->lineno())
    , m_op(op)
    , m_is_lvalue(false)
{
    addchild(a);
#ifndef NDEBUG
    node_counts[nodetype] += 1;
    node_counts_peak[nodetype] += 1;
#endif
}



ASTNode::ASTNode(NodeType nodetype, OSLCompilerImpl* compiler, int op)
    : m_nodetype(nodetype)
    , m_compiler(compiler)
    , m_sourcefile(compiler->filename())
    , m_sourceline(compiler->lineno())
    , m_op(op)
    , m_is_lvalue(false)
{
#ifndef NDEBUG
    node_counts[nodetype] += 1;
    node_counts_peak[nodetype] += 1;
#endif
}



ASTNode::ASTNode(NodeType nodetype, OSLCompilerImpl* compiler, int op,
                 ASTNode* a, ASTNode* b)
    : m_nodetype(nodetype)
    , m_compiler(compiler)
    , m_sourcefile(compiler->filename())
    , m_sourceline(compiler->lineno())
    , m_op(op)
    , m_is_lvalue(false)
{
    addchild(a);
    addchild(b);
#ifndef NDEBUG
    node_counts[nodetype] += 1;
    node_counts_peak[nodetype] += 1;
#endif
}



ASTNode::ASTNode(NodeType nodetype, OSLCompilerImpl* compiler, int op,
                 ASTNode* a, ASTNode* b, ASTNode* c)
    : m_nodetype(nodetype)
    , m_compiler(compiler)
    , m_sourcefile(compiler->filename())
    , m_sourceline(compiler->lineno())
    , m_op(op)
    , m_is_lvalue(false)
{
    addchild(a);
    addchild(b);
    addchild(c);
#ifndef NDEBUG
    node_counts[nodetype] += 1;
    node_counts_peak[nodetype] += 1;
#endif
}



ASTNode::ASTNode(NodeType nodetype, OSLCompilerImpl* compiler, int op,
                 ASTNode* a, ASTNode* b, ASTNode* c, ASTNode* d)
    : m_nodetype(nodetype)
    , m_compiler(compiler)
    , m_sourcefile(compiler->filename())
    , m_sourceline(compiler->lineno())
    , m_op(op)
    , m_is_lvalue(false)
{
    addchild(a);
    addchild(b);
    addchild(c);
    addchild(d);
#ifndef NDEBUG
    node_counts[nodetype] += 1;
    node_counts_peak[nodetype] += 1;
#endif
}



ASTNode::~ASTNode()
{
    // For sufficiently deep trees, the recursive deletion of nodes could
    // overflow the stack. So do it sequentially.
    while (m_next) {
        // Currently:
        //     m_next -->  A  --> B --> ...
        ref n = m_next;
        // Now:
        //     n --> A --> B --> ...
        //     m_next -->  A  --> B --> ...
        m_next = n->m_next;
        // Now:
        //     n --> A --> B --> ...
        //     m_next -->  B --> ...
        n->m_next.reset();
        // Now:
        //     n --> A
        //     m_next -->  B --> ...
        // When we loop, n will exit scope and we will have
        //     m_next --> B --> ...
        // A will have been freed, next time through the loop we will free
        // B, and there was no recursion.
    }

#ifndef NDEBUG
    node_counts[nodetype()] -= 1;
#endif
}



void
ASTNode::error_impl(string_view msg) const
{
    m_compiler->errorfmt(sourcefile(), sourceline(), "{}", msg);
}



void
ASTNode::warning_impl(string_view msg) const
{
    m_compiler->warningfmt(sourcefile(), sourceline(), "{}", msg);
}



void
ASTNode::info_impl(string_view msg) const
{
    m_compiler->infofmt(sourcefile(), sourceline(), "{}", msg);
}



void
ASTNode::message_impl(string_view msg) const
{
    m_compiler->messagefmt(sourcefile(), sourceline(), "{}", msg);
}



void
ASTNode::print(std::ostream& out, int indentlevel) const
{
    indent(out, indentlevel);
    OSL::print(out, "({} :     (type: {}) {}\n", nodetypename(), typespec(),
               (opname() ? opname() : ""));
    printchildren(out, indentlevel);
    indent(out, indentlevel);
    OSL::print(out, ")\n");
}



void
ASTNode::printchildren(std::ostream& out, int indentlevel) const
{
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (!child(i))
            continue;
        indent(out, indentlevel);
        if (childname(i))
            OSL::print(out, "  {}", childname(i));
        else
            OSL::print(out, "  child{}", i);
        OSL::print(out, ": ");
        if (typespec() != TypeSpec() && !child(i)->next())
            OSL::print(out, " (type: {})", typespec());
        OSL::print(out, "\n");
        printlist(out, child(i), indentlevel + 1);
    }
}



const char*
ASTNode::type_c_str(const TypeSpec& type) const
{
    return m_compiler->type_c_str(type);
}



void
ASTNode::list_to_vec(const ref& A, std::vector<ref>& vec)
{
    vec.clear();
    for (ref node = A; node; node = node->next())
        vec.push_back(node);
}



ASTNode::ref
ASTNode::vec_to_list(std::vector<ref>& vec)
{
    if (vec.size()) {
        for (size_t i = 0; i < vec.size() - 1; ++i)
            vec[i]->m_next = vec[i + 1];
        vec[vec.size() - 1]->m_next = NULL;
        return vec[0];
    } else {
        return ref();
    }
}



std::string
ASTNode::list_to_types_string(const ASTNode* node)
{
    std::ostringstream result;
    for (int i = 0; node; node = node->nextptr(), ++i)
        OSL::print(result, "{}{}", i ? ", " : "", node->typespec());
    return result.str();
}



ASTshader_declaration::ASTshader_declaration(OSLCompilerImpl* comp, int stype,
                                             ustring name, ASTNode* form,
                                             ASTNode* stmts, ASTNode* meta)
    : ASTNode(shader_declaration_node, comp, stype, meta, form, stmts)
    , m_shadername(name)
{
    // Double check some requirements of shader parameters
    for (ASTNode* arg = form; arg; arg = arg->nextptr()) {
        OSL_ASSERT(arg->nodetype() == variable_declaration_node);
        ASTvariable_declaration* v = (ASTvariable_declaration*)arg;
        if (!v->init())
            v->errorfmt("shader parameter '{}' requires a default initializer",
                        v->name());
        if (v->is_output() && v->typespec().is_unsized_array())
            v->errorfmt("shader output parameter '{}' can't be unsized array",
                        v->name());
    }
}



const char*
ASTshader_declaration::childname(size_t i) const
{
    static const char* name[] = { "metadata", "formals", "statements" };
    return name[i];
}



void
ASTshader_declaration::print(std::ostream& out, int indentlevel) const
{
    indent(out, indentlevel);
    OSL::print(out, "({} {} \"{}\"\n", nodetypename(), shadertypename(),
               m_shadername);
    printchildren(out, indentlevel);
    indent(out, indentlevel);
    OSL::print(out, ")\n");
}



string_view
ASTshader_declaration::shadertypename() const
{
    return OSL::pvt::shadertypename((ShaderType)m_op);
}



ASTfunction_declaration::ASTfunction_declaration(OSLCompilerImpl* comp,
                                                 TypeSpec type, ustring name,
                                                 ASTNode* form, ASTNode* stmts,
                                                 ASTNode* meta,
                                                 int sourceline_start)
    : ASTNode(function_declaration_node, comp, 0, meta, form, stmts)
    , m_name(name)
    , m_sym(NULL)
    , m_is_builtin(false)
{
    // Some trickery -- the compiler's idea of the "current" source line
    // is the END of the function body, so if a hint was passed about the
    // start of the declaration, substitute that.
    if (sourceline_start >= 0)
        m_sourceline = sourceline_start;

    if (Strutil::starts_with(name, "___"))
        errorfmt("\"{}\" : sorry, can't start with three underscores", name);

    // Get a pointer to the first of the existing symbols of that name.
    Symbol* existing_syms = comp->symtab().clash(name);
    if (existing_syms && existing_syms->symtype() != SymTypeFunction) {
        errorfmt("\"{}\" already declared in this scope as a {}", name,
                 existing_syms->typespec());
        // FIXME -- print the file and line of the other definition
        existing_syms = NULL;
    }

    // Build up the argument signature for this declared function
    m_typespec           = type;
    std::string argcodes = m_compiler->code_from_type(m_typespec);
    for (ASTNode* arg = form; arg; arg = arg->nextptr()) {
        const TypeSpec& t(arg->typespec());
        if (t == TypeSpec() /* UNKNOWN */) {
            m_typespec = TypeDesc::UNKNOWN;
            return;
        }
        argcodes += m_compiler->code_from_type(t);
        OSL_ASSERT(arg->nodetype() == variable_declaration_node);
        ASTvariable_declaration* v = (ASTvariable_declaration*)arg;
        if (v->init())
            v->errorfmt(
                "function parameter '{}' may not have a default initializer.",
                v->name());
    }

    // Allow multiple function declarations, but only if they aren't the
    // same polymorphic type in the same scope.
    if (stmts) {
        std::string err;
        int current_scope = m_compiler->symtab().scopeid();
        for (FunctionSymbol* f = static_cast<FunctionSymbol*>(existing_syms); f;
             f                 = f->nextpoly()) {
            if (f->scope() == current_scope && f->argcodes() == argcodes) {
                // If the argcodes match, only one should have statements.
                // If there is no ASTNode for the poly, must be a builtin, and
                // has 'implicit' statements.
                auto other = static_cast<ASTfunction_declaration*>(f->node());
                if (!other || (other->statements() || other->is_builtin())) {
                    if (err.empty()) {
                        err = Strutil::fmt::format(
                            "Function '{} {} ({})' redefined in the same scope\n"
                            "  Previous definitions:",
                            type, name, list_to_types_string(form));
                    }
                    err += "\n    ";
                    if (other) {
                        err += Strutil::fmt::format(
                            "{}:{}",
                            OIIO::Filesystem::filename(
                                other->sourcefile().string()),
                            other->sourceline());
                    } else
                        err += "built-in";
                }
            }
        }
        if (!err.empty())
            warningfmt("{}", err);
    }


    m_sym = new FunctionSymbol(name, type, this);
    func()->nextpoly((FunctionSymbol*)existing_syms);

    func()->argcodes(ustring(argcodes));
    m_compiler->symtab().insert(m_sym);

    // Typecheck it right now, upon declaration
    typecheck(typespec());
}



void
ASTfunction_declaration::add_meta(ref metaref)
{
    for (ASTNode* meta = metaref.get(); meta; meta = meta->nextptr()) {
        OSL_ASSERT(meta->nodetype() == ASTNode::variable_declaration_node);
        const ASTvariable_declaration* metavar
            = static_cast<const ASTvariable_declaration*>(meta);
        Symbol* metasym = metavar->sym();
        if (metasym->name() == "builtin") {
            m_is_builtin = true;
            if (func()->typespec().is_closure()) {  // It is a builtin closure
                // Force keyword arguments at the end
                func()->argcodes(
                    ustring(std::string(func()->argcodes().c_str()) + "."));
            }
            // For built-in functions, if any of the params are output,
            // also automatically mark it as readwrite_special_case.
            for (ASTNode* f = formals().get(); f; f = f->nextptr()) {
                OSL_ASSERT(f->nodetype() == variable_declaration_node);
                ASTvariable_declaration* v = (ASTvariable_declaration*)f;
                if (v->is_output())
                    func()->readwrite_special_case(true);
            }
        } else if (metasym->name() == "derivs")
            func()->takes_derivs(true);
        else if (metasym->name() == "printf_args")
            func()->printf_args(true);
        else if (metasym->name() == "texture_args")
            func()->texture_args(true);
        else if (metasym->name() == "rw")
            func()->readwrite_special_case(true);
    }
}



const char*
ASTfunction_declaration::childname(size_t i) const
{
    static const char* name[] = { "metadata", "formals", "statements" };
    return name[i];
}



void
ASTfunction_declaration::print(std::ostream& out, int indentlevel) const
{
    indent(out, indentlevel);
    OSL::print(out, "{} {}", nodetypename(), m_sym->mangled());
    if (m_sym->scope())
        OSL::print(out, " ({} in scope {})", m_sym->name(), m_sym->scope());
    OSL::print(out, "\n");
    printchildren(out, indentlevel);
}



ASTvariable_declaration::ASTvariable_declaration(OSLCompilerImpl* comp,
                                                 const TypeSpec& type,
                                                 ustring name, ASTNode* init,
                                                 bool isparam, bool ismeta,
                                                 bool isoutput, bool initlist,
                                                 int sourceline_start)
    : ASTNode(variable_declaration_node, comp, 0, init, NULL /* meta */)
    , m_name(name)
    , m_sym(NULL)
    , m_isparam(isparam)
    , m_isoutput(isoutput)
    , m_ismetadata(ismeta)
    , m_initlist(initlist)
{
    // Some trickery -- the compiler's idea of the "current" source line
    // is the END of the declaration, so if a hint was passed about the
    // start of the declaration, substitute that.
    if (sourceline_start >= 0)
        m_sourceline = sourceline_start;

    if (m_initlist && init) {
        // Typecheck the init list early.
        OSL_ASSERT(init->nodetype() == compound_initializer_node);
        static_cast<ASTcompound_initializer*>(init)->typecheck(type);
    }

    m_typespec = type;
    Symbol* f  = comp->symtab().clash(name);
    if (f && !m_ismetadata) {
        std::string e
            = Strutil::fmt::format("\"{}\" already declared in this scope",
                                   name);
        if (f->node()) {
            std::string filename = OIIO::Filesystem::filename(
                f->node()->sourcefile().string());
            e += Strutil::fmt::format("\n\t\tprevious declaration was at {}:{}",
                                      filename, f->node()->sourceline());
        }
        if (f->scope() == 0 && f->symtype() == SymTypeFunction && isparam) {
            // special case: only a warning for param to mask global function
            warningfmt("{}", e);
        } else {
            errorfmt("{}", e);
        }
    }
    if (OIIO::Strutil::starts_with(name, "___")) {
        errorfmt("\"{}\" : sorry, can't start with three underscores", name);
    }
    SymType symtype = isparam ? (isoutput ? SymTypeOutputParam : SymTypeParam)
                              : SymTypeLocal;
    // Sneaky debugging aid: a local that starts with "__debug_tmp__"
    // gets declared as a temp. Don't do this on purpose!!!
    if (symtype == SymTypeLocal && Strutil::starts_with(name, "__debug_tmp__"))
        symtype = SymTypeTemp;
    m_sym = new Symbol(name, type, symtype, this);
    if (m_ismetadata) {
        // Metadata doesn't go in the symbol table, so we need to retain
        // an owning pointer so it doesn't leak.
        m_sym_owned.reset(m_sym);
    } else {
        // Put in the symbol table, after which symtab is the owner
        m_compiler->symtab().insert(m_sym);
    }

    // A struct really makes several subvariables
    if (type.is_structure() || type.is_structure_array()) {
        OSL_ASSERT(!m_ismetadata);
        // Add the fields as individual declarations
        m_compiler->add_struct_fields(type.structspec(), m_sym->name(), symtype,
                                      type.is_unsized_array()
                                          ? -1
                                          : type.arraylength(),
                                      this, init);
    }
}



const char*
ASTvariable_declaration::nodetypename() const
{
    return m_isparam ? "parameter" : "variable_declaration";
}



const char*
ASTvariable_declaration::childname(size_t i) const
{
    static const char* name[] = { "initializer", "metadata" };
    return name[i];
}



void
ASTvariable_declaration::print(std::ostream& out, int indentlevel) const
{
    indent(out, indentlevel);
    OSL::print(out, "({} {}", nodetypename(), m_sym->mangled());
#if 0
    if (m_sym->scope())
        OSL::print(out, " ({} in scope {})", m_sym->name(), m_sym->scope());
#endif
    OSL::print(out, "\n");
    printchildren(out, indentlevel);
    indent(out, indentlevel);
    OSL::print(out, ")\n");
}



ASTvariable_ref::ASTvariable_ref(OSLCompilerImpl* comp, ustring name)
    : ASTNode(variable_ref_node, comp), m_name(name), m_sym(NULL)
{
    m_sym = comp->symtab().find(name);
    if (!m_sym) {
        errorfmt("'{}' was not declared in this scope", name);
        // FIXME -- would be fun to troll through the symtab and try to
        // find the things that almost matched and offer suggestions.
        return;
    }
    if (m_sym->symtype() == SymTypeFunction) {
        errorfmt("function '{}' can't be used as a variable", name);
        return;
    }
    if (m_sym->symtype() == SymTypeType) {
        errorfmt("type name '{}' can't be used as a variable", name);
        return;
    }
    m_typespec = m_sym->typespec();
}



void
ASTvariable_ref::print(std::ostream& out, int indentlevel) const
{
    indent(out, indentlevel);
    OSL::print(out, "({} (type: {}) {})\n", nodetypename(),
               m_sym ? m_sym->typespec().string() : "unknown",
               m_sym ? m_sym->mangled() : m_name.string());
    OSL_DASSERT(nchildren() == 0);
}



ASTpreincdec::ASTpreincdec(OSLCompilerImpl* comp, int op, ASTNode* expr)
    : ASTNode(preincdec_node, comp, op, expr)
{
    check_symbol_writeability(expr);
}



const char*
ASTpreincdec::childname(size_t i) const
{
    static const char* name[] = { "expression" };
    return name[i];
}



ASTpostincdec::ASTpostincdec(OSLCompilerImpl* comp, int op, ASTNode* expr)
    : ASTNode(postincdec_node, comp, op, expr)
{
    check_symbol_writeability(expr);
}



const char*
ASTpostincdec::childname(size_t i) const
{
    static const char* name[] = { "expression" };
    return name[i];
}



ASTindex::ASTindex(OSLCompilerImpl* comp, ASTNode* expr, ASTNode* index,
                   ASTNode* index2, ASTNode* index3)
    : ASTNode(index_node, comp, 0, expr, index /*NO: index2, index3*/)
{
    // We only initialized the first child. Add more if additional arguments
    // were supplied.
    OSL_DASSERT(index);
    if (index2)
        addchild(index2);
    if (index3)
        addchild(index3);

    // Special case: an ASTindex where the `expr` is itself an ASTindex.
    // This construction results from named-component access of array
    // elements, e.g., `colorarray[i].r`. In that case, what we want to do
    // is rearrange to turn this into the two-index variety and discard the
    // child index.
    if (!index2 && expr->nodetype() == index_node && expr->nchildren() == 2) {
        ref newexpr   = static_cast<ASTindex*>(expr)->lvalue();
        ref newindex  = static_cast<ASTindex*>(expr)->index();
        ref newindex2 = index;
        clearchildren();
        addchild(newexpr);
        expr = newexpr.get();
        addchild(newindex);
        index = newindex.get();
        addchild(newindex2);
        index2 = newindex2.get();
    }

    OSL_DASSERT(expr->nodetype() == variable_ref_node
                || expr->nodetype() == structselect_node);
    OSL_DASSERT(m_typespec.is_unknown());

    if (!index2) {
        // 1-argument: simple array a[i] or component dereference triple[c]
        if (expr->typespec().is_array())  // array dereference
            m_typespec = expr->typespec().elementtype();
        else if (!expr->typespec().is_closure()
                 && expr->typespec().is_triple())  // component access
            m_typespec = TypeDesc::FLOAT;
    } else if (!index3) {
        // 2-argument: matrix dereference m[r][c], or component of a
        // triple array colorarray[i][c].
        if (expr->typespec().is_matrix())  // matrix component access
            m_typespec = TypeDesc::FLOAT;
        else if (expr->typespec().is_array() &&  // triplearray[][]
                 expr->typespec().elementtype().is_triple())
            m_typespec = TypeDesc::FLOAT;
    } else {
        // 3-argument: one component of an array of matrices
        // matrixarray[i][r][c]
        if (expr->typespec().is_array() &&  // matrixarray[][]
            expr->typespec().elementtype().is_matrix())
            m_typespec = TypeDesc::FLOAT;
    }

    if (m_typespec.is_unknown()) {
        errorfmt("indexing into non-array or non-component type");
    }
}



const char*
ASTindex::childname(size_t i) const
{
    static const char* name[] = { "expression", "index", "index" };
    return name[i];
}



ASTstructselect::ASTstructselect(OSLCompilerImpl* comp, ASTNode* expr,
                                 ustring field)
    : ASTNode(structselect_node, comp, 0, expr)
    , m_field(field)
    , m_structid(-1)
    , m_fieldid(-1)
    , m_fieldname(field)
    , m_fieldsym(NULL)
{
    m_fieldsym = find_fieldsym(m_structid, m_fieldid);
    if (m_fieldsym) {
        m_fieldname = m_fieldsym->name();
        m_typespec  = m_fieldsym->typespec();
    } else if (m_compindex) {
        // It's a named component, like point.x
        m_typespec = OIIO::TypeFloat;  // These cases are always single floats
    }
}



/// Return the symbol pointer to the individual field that this
/// structselect represents; also set structid to the ID of the
/// structure type, and fieldid to the field index within the struct.
Symbol*
ASTstructselect::find_fieldsym(int& structid, int& fieldid)
{
    auto lv     = lvalue().get();
    auto lvtype = lv->typespec();

    if (lvtype.is_color()
        && (fieldname() == "r" || fieldname() == "g" || fieldname() == "b")) {
        OSL_DASSERT(fieldid == -1 && !m_compindex);
        fieldid = fieldname() == "r" ? 0 : (fieldname() == "g" ? 1 : 2);
        m_compindex.reset(
            new ASTindex(m_compiler, lv, new ASTliteral(m_compiler, fieldid)));
        m_is_lvalue = true;
        return nullptr;
    } else if (lvtype.is_vectriple()
               && (fieldname() == "x" || fieldname() == "y"
                   || fieldname() == "z")) {
        OSL_DASSERT(fieldid == -1 && !m_compindex);
        fieldid = fieldname() == "x" ? 0 : (fieldname() == "y" ? 1 : 2);
        m_compindex.reset(
            new ASTindex(m_compiler, lv, new ASTliteral(m_compiler, fieldid)));
        m_is_lvalue = true;
        return nullptr;
    }

    if (!lvtype.is_structure() && !lvtype.is_structure_array()) {
        errorfmt("type '{}' does not have a member '{}'", lvtype, m_field);
        return NULL;
    }

    ustring structsymname;
    TypeSpec structtype;
    find_structsym(lvalue().get(), structsymname, structtype);

    structid = structtype.structure();
    StructSpec* structspec(structtype.structspec());
    fieldid = -1;
    for (int i = 0; i < (int)structspec->numfields(); ++i) {
        if (structspec->field(i).name == m_field) {
            fieldid = i;
            break;
        }
    }

    if (fieldid < 0) {
        errorfmt("struct type '{}' does not have a member '{}'",
                 structspec->name(), m_field);
        return NULL;
    }

    const StructSpec::FieldSpec& fieldrec(structspec->field(fieldid));
    ustring fieldsymname = ustring::fmtformat("{}.{}", structsymname,
                                              fieldrec.name);
    Symbol* sym          = m_compiler->symtab().find(fieldsymname);
    return sym;
}



/// structnode is an AST node representing a struct.  It could be a
/// struct variable, or a field of a struct (which is itself a struct),
/// or an array element of a struct.  Whatever, here we figure out some
/// vital information about it: the name of the symbol representing the
/// struct, and its type.
void
ASTstructselect::find_structsym(ASTNode* structnode, ustring& structname,
                                TypeSpec& structtype)
{
    // This node selects a field from a struct. The purpose of this
    // method is to "flatten" the possibly-nested (struct in struct, and
    // or array of structs) down to a symbol that represents the
    // particular field.  In the process, we set structname and its
    // type structtype.
    OSL_DASSERT(structnode->typespec().is_structure()
                || structnode->typespec().is_structure_array());
    if (structnode->nodetype() == variable_ref_node) {
        // The structnode is a top-level struct variable
        ASTvariable_ref* var = (ASTvariable_ref*)structnode;
        structname           = var->name();
        structtype           = var->typespec();
    } else if (structnode->nodetype() == structselect_node) {
        // The structnode is itself a field of another struct.
        ASTstructselect* thestruct = (ASTstructselect*)structnode;
        int structid, fieldid;
        Symbol* sym = thestruct->find_fieldsym(structid, fieldid);
        structname  = sym->name();
        structtype  = sym->typespec();
    } else if (structnode->nodetype() == index_node) {
        // The structnode is an element of an array of structs:
        ASTindex* arrayref = (ASTindex*)structnode;
        find_structsym(arrayref->lvalue().get(), structname, structtype);
        structtype.make_array(0);  // clear its arrayness
    } else {
        OSL_ASSERT(0 && "Malformed ASTstructselect");
    }
}



const char*
ASTstructselect::childname(size_t i) const
{
    static const char* name[] = { "structure" };
    return name[i];
}



void
ASTstructselect::print(std::ostream& out, int indentlevel) const
{
    ASTNode::print(out, indentlevel);
    indent(out, indentlevel + 1);
    OSL::print(out, "select {}\n", field());
}



const char*
ASTconditional_statement::childname(size_t i) const
{
    static const char* name[] = { "condition", "truestatement",
                                  "falsestatement" };
    return name[i];
}



ASTloop_statement::ASTloop_statement(OSLCompilerImpl* comp, LoopType looptype,
                                     ASTNode* init, ASTNode* cond,
                                     ASTNode* iter, ASTNode* stmt)
    : ASTNode(loop_statement_node, comp, looptype, init, cond, iter, stmt)
{
    // Handle empty comparison, for(;;), is same as for(;1;)
    if (!cond)
        m_children[1] = new ASTliteral(comp, 1);
}



const char*
ASTloop_statement::childname(size_t i) const
{
    static const char* name[] = { "initializer", "condition", "iteration",
                                  "bodystatement" };
    return name[i];
}



const char*
ASTloop_statement::opname() const
{
    switch (m_op) {
    case LoopWhile: return "while";
    case LoopDo: return "dowhile";
    case LoopFor: return "for";
    default: OSL_ASSERT(0 && "unknown loop type"); return "unknown";
    }
}



const char*
ASTloopmod_statement::childname(size_t /*i*/) const
{
    return NULL;  // no children
}



const char*
ASTloopmod_statement::opname() const
{
    switch (m_op) {
    case LoopModBreak: return "break";
    case LoopModContinue: return "continue";
    default: OSL_ASSERT(0 && "unknown loop modifier"); return "unknown";
    }
}



const char*
ASTreturn_statement::childname(size_t /*i*/) const
{
    return "expression";  // only child
}



ASTcompound_initializer::ASTcompound_initializer(OSLCompilerImpl* comp,
                                                 ASTNode* exprlist)
    : ASTtype_constructor(compound_initializer_node, comp, TypeSpec(), exprlist)
    , m_ctor(false)
{
}



const char*
ASTcompound_initializer::childname(size_t /*i*/) const
{
    return canconstruct() ? "args" : "expression_list";
}



bool
ASTNode::check_symbol_writeability(ASTNode* var, bool quiet, Symbol** dest_sym)
{
    if (dest_sym)
        *dest_sym = nullptr;
    if (var->nodetype() == index_node)
        return check_symbol_writeability(
            static_cast<ASTindex*>(var)->lvalue().get());
    if (var->nodetype() == structselect_node)
        return check_symbol_writeability(
            static_cast<ASTstructselect*>(var)->lvalue().get());

    Symbol* dest = nullptr;
    if (var->nodetype() == variable_ref_node)
        dest = static_cast<ASTvariable_ref*>(var)->sym();
    else if (var->nodetype() == variable_declaration_node)
        dest = static_cast<ASTvariable_declaration*>(var)->sym();

    if (dest) {
        if (dest_sym)
            *dest_sym = dest;
        if (dest->readonly()) {
            if (!quiet)
                warningfmt("cannot write to non-output parameter \"{}\"",
                           dest->name());
            // Note: Consider it only a warning to write to a non-output
            // parameter. Users who want it to be a hard error can use
            // -Werror. Writing to any other readonly symbols is a full
            // error.
            return false;
        }
    } else {
        // OSL::print("Don't know how to check_symbol_writeability {}\n",
        //            var->nodetypename());
    }
    return true;
}



ASTassign_expression::ASTassign_expression(OSLCompilerImpl* comp, ASTNode* var,
                                           Operator op, ASTNode* expr)
    : ASTNode(assign_expression_node, comp, op, var, expr)
{
    if (op != Assign) {
        // Rejigger to straight assignment and binary op
        m_op          = Assign;
        m_children[1] = new ASTbinary_expression(comp, op, var, expr);
    }

    check_symbol_writeability(var);
}



const char*
ASTassign_expression::childname(size_t i) const
{
    static const char* name[] = { "variable", "expression" };
    return name[i];
}



const char*
ASTassign_expression::opname() const
{
    // clang-format off
    switch (m_op) {
    case Assign     : return "=";
    case Mul        : return "*=";
    case Div        : return "/=";
    case Add        : return "+=";
    case Sub        : return "-=";
    case BitAnd     : return "&=";
    case BitOr      : return "|=";
    case Xor        : return "^=";
    case ShiftLeft  : return "<<=";
    case ShiftRight : return ">>=";
    default:
        OSL_ASSERT (0 && "unknown assignment expression");
        return "="; // punt
    }
    // clang-format on
}



const char*
ASTassign_expression::opword() const
{
    // clang-format off
    switch (m_op) {
    case Assign     : return "assign";
    case Mul        : return "mul";
    case Div        : return "div";
    case Add        : return "add";
    case Sub        : return "sub";
    case BitAnd     : return "bitand";
    case BitOr      : return "bitor";
    case Xor        : return "xor";
    case ShiftLeft  : return "shl";
    case ShiftRight : return "shr";
    default:
        OSL_ASSERT (0 && "unknown assignment expression");
        return "assign"; // punt
    }
    // clang-format on
}



ASTunary_expression::ASTunary_expression(OSLCompilerImpl* comp, int op,
                                         ASTNode* expr)
    : ASTNode(unary_expression_node, comp, op, expr)
{
    // Check for a user-overloaded function for this operator
    Symbol* sym = comp->symtab().find(
        ustring::fmtformat("__operator__{}__", opword()));
    if (sym && sym->symtype() == SymTypeFunction)
        m_function_overload = (FunctionSymbol*)sym;
}



const char*
ASTunary_expression::childname(size_t i) const
{
    static const char* name[] = { "expression" };
    return name[i];
}



const char*
ASTunary_expression::opname() const
{
    switch (m_op) {
    case Add: return "+";
    case Sub: return "-";
    case Not: return "!";
    case Compl: return "~";
    default: OSL_ASSERT(0 && "unknown unary expression"); return "unknown";
    }
}



const char*
ASTunary_expression::opword() const
{
    switch (m_op) {
    case Add: return "add";
    case Sub: return "neg";
    case Not: return "not";
    case Compl: return "compl";
    default: OSL_ASSERT(0 && "unknown unary expression"); return "unknown";
    }
}



ASTbinary_expression::ASTbinary_expression(OSLCompilerImpl* comp, Operator op,
                                           ASTNode* left, ASTNode* right)
    : ASTNode(binary_expression_node, comp, op, left, right)
{
    // Check for a user-overloaded function for this operator.
    // Disallow a few ops from overloading.
    if (op != And && op != Or) {
        ustring funcname = ustring::fmtformat("__operator__{}__", opword());
        Symbol* sym      = comp->symtab().find(funcname);
        if (sym && sym->symtype() == SymTypeFunction)
            m_function_overload = (FunctionSymbol*)sym;
    }
}



ASTNode*
ASTbinary_expression::make(OSLCompilerImpl* comp, Operator op, ASTNode* left,
                           ASTNode* right)
{
    // If the left and right are both literal constants, fold the expression
    if (left->nodetype() == literal_node && right->nodetype() == literal_node) {
        ASTNode* cf = nullptr;  // constant-folded result
        if (left->typespec().is_int() && right->typespec().is_int()) {
            int lv = dynamic_cast<ASTliteral*>(left)->intval();
            int rv = dynamic_cast<ASTliteral*>(right)->intval();
            switch (op) {
            case Mul: cf = new ASTliteral(comp, lv * rv); break;
            case Div: cf = new ASTliteral(comp, rv ? lv / rv : 0); break;
            case Add: cf = new ASTliteral(comp, lv + rv); break;
            case Sub: cf = new ASTliteral(comp, lv - rv); break;
            case Mod: cf = new ASTliteral(comp, rv ? lv % rv : 0); break;
            case Equal: cf = new ASTliteral(comp, lv == rv ? 1 : 0); break;
            case NotEqual: cf = new ASTliteral(comp, lv != rv ? 1 : 0); break;
            case Greater: cf = new ASTliteral(comp, lv > rv ? 1 : 0); break;
            case Less: cf = new ASTliteral(comp, lv < rv ? 1 : 0); break;
            case GreaterEqual:
                cf = new ASTliteral(comp, lv >= rv ? 1 : 0);
                break;
            case LessEqual: cf = new ASTliteral(comp, lv <= rv ? 1 : 0); break;
            case BitAnd: cf = new ASTliteral(comp, lv & rv); break;
            case BitOr: cf = new ASTliteral(comp, lv | rv); break;
            case Xor: cf = new ASTliteral(comp, lv ^ rv); break;
            case ShiftLeft: cf = new ASTliteral(comp, lv << rv); break;
            case ShiftRight: cf = new ASTliteral(comp, lv >> rv); break;
            default: break;
            }
        } else if (left->typespec().is_float()
                   && right->typespec().is_float()) {
            float lv = dynamic_cast<ASTliteral*>(left)->floatval();
            float rv = dynamic_cast<ASTliteral*>(right)->floatval();
            switch (op) {
            case Mul: cf = new ASTliteral(comp, lv * rv); break;
            case Div: cf = new ASTliteral(comp, rv ? lv / rv : 0.0f); break;
            case Add: cf = new ASTliteral(comp, lv + rv); break;
            case Sub: cf = new ASTliteral(comp, lv - rv); break;
            case Equal: cf = new ASTliteral(comp, lv == rv ? 1 : 0); break;
            case NotEqual: cf = new ASTliteral(comp, lv != rv ? 1 : 0); break;
            case Greater: cf = new ASTliteral(comp, lv > rv ? 1 : 0); break;
            case Less: cf = new ASTliteral(comp, lv < rv ? 1 : 0); break;
            case GreaterEqual:
                cf = new ASTliteral(comp, lv >= rv ? 1 : 0);
                break;
            case LessEqual: cf = new ASTliteral(comp, lv <= rv ? 1 : 0); break;
            default: break;
            }
        }
        if (cf) {
            delete left;
            delete right;
            return cf;
        }
    }

    return new ASTbinary_expression(comp, op, left, right);
}



const char*
ASTbinary_expression::childname(size_t i) const
{
    static const char* name[] = { "left", "right" };
    return name[i];
}



const char*
ASTbinary_expression::opname() const
{
    // clang-format off
    switch (m_op) {
    case Mul          : return "*";
    case Div          : return "/";
    case Add          : return "+";
    case Sub          : return "-";
    case Mod          : return "%";
    case Equal        : return "==";
    case NotEqual     : return "!=";
    case Greater      : return ">";
    case GreaterEqual : return ">=";
    case Less         : return "<";
    case LessEqual    : return "<=";
    case BitAnd       : return "&";
    case BitOr        : return "|";
    case Xor          : return "^";
    case And          : return "&&";
    case Or           : return "||";
    case ShiftLeft    : return "<<";
    case ShiftRight   : return ">>";
    default:
        OSL_ASSERT (0 && "unknown binary expression");
        return "unknown";
    }
    // clang-format on
}



const char*
ASTbinary_expression::opword() const
{
    // clang-format off
    switch (m_op) {
    case Mul          : return "mul";
    case Div          : return "div";
    case Add          : return "add";
    case Sub          : return "sub";
    case Mod          : return "mod";
    case Equal        : return "eq";
    case NotEqual     : return "neq";
    case Greater      : return "gt";
    case GreaterEqual : return "ge";
    case Less         : return "lt";
    case LessEqual    : return "le";
    case BitAnd       : return "bitand";
    case BitOr        : return "bitor";
    case Xor          : return "xor";
    case And          : return "and";
    case Or           : return "or";
    case ShiftLeft    : return "shl";
    case ShiftRight   : return "shr";
    default:
        OSL_ASSERT (0 && "unknown binary expression");
        return "unknown";
    }
    // clang-format on
}



const char*
ASTternary_expression::childname(size_t i) const
{
    static const char* name[] = { "condition", "trueexpression",
                                  "falseexpression" };
    return name[i];
}



const char*
ASTtypecast_expression::childname(size_t i) const
{
    static const char* name[] = { "expr" };
    return name[i];
}



const char*
ASTtype_constructor::childname(size_t i) const
{
    static const char* name[] = { "args" };
    return name[i];
}



ASTfunction_call::ASTfunction_call(OSLCompilerImpl* comp, ustring name,
                                   ASTNode* args, FunctionSymbol* funcsym)
    : ASTNode(function_call_node, comp, 0, args)
    , m_name(name)
    , m_sym(funcsym ? funcsym : comp->symtab().find(name))  // Look it up.
    , m_poly(funcsym)      // Default - resolved symbol or null
    , m_argread(~1)        // Default - all args are read except the first
    , m_argwrite(1)        // Default - first arg only is written by the op
    , m_argtakesderivs(0)  // Default - doesn't take derivs
{
    if (!m_sym) {
        errorfmt("function '{}' was not declared in this scope", name);
        // FIXME -- would be fun to troll through the symtab and try to
        // find the things that almost matched and offer suggestions.
        return;
    }
    if (is_struct_ctr()) {
        return;  // It's a struct constructor
    }
    if (m_sym->symtype() != SymTypeFunction) {
        errorfmt("'{}' is not a function", name);
        m_sym = NULL;
        return;
    }
}



const char*
ASTfunction_call::childname(size_t i) const
{
    return ustring::fmtformat("param{}", (int)i).c_str();
}



const char*
ASTfunction_call::opname() const
{
    return m_name.c_str();
}



void
ASTfunction_call::print(std::ostream& out, int indentlevel) const
{
    ASTNode::print(out, indentlevel);
#if 0
    if (is_user_function()) {
        OSL::print(out, "\n");
        user_function()->print (out, indentlevel+1);
        OSL::print(out, "\n");
    }
#endif
}



const char*
ASTliteral::childname(size_t /*i*/) const
{
    return NULL;
}



void
ASTliteral::print(std::ostream& out, int indentlevel) const
{
    indent(out, indentlevel);
    OSL::print(out, "({} (type: {}) ", nodetypename(), m_typespec);
    if (m_typespec.is_int())
        OSL::print(out, "{}", m_i);
    else if (m_typespec.is_float())
        OSL::print(out, "{}", m_f);
    else if (m_typespec.is_string())
        OSL::print(out, "\"{}\"", m_s);
    OSL::print(out, ")\n");
}


};  // namespace pvt

OSL_NAMESPACE_EXIT
