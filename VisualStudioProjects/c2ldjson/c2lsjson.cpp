#include <iostream>
#include <string>
#include <deque>
#include <algorithm>
#include "clang-c/Index.h"

using namespace std;

class Context {
public:
   /*
    * TODO: Truck current function and add USR reference to params
    */
   CXTranslationUnit translation_unit;
   deque<string> nested_namespaces;
   deque<string> nested_types;

   string current_namespace() {
      if (nested_namespaces.empty()) {
         return "";
      }

      return nested_namespaces.back();
   }

   string current_type() {
      if (nested_types.empty()) {
         return "";
      }

      return nested_types.back();
   }
};

class LocationDescription {
public:
   const char * file;
   unsigned int line;
   unsigned int column;
   unsigned int offset;

   LocationDescription(CXCursor cursor) {
      CXSourceLocation location = clang_getCursorLocation(cursor);
      CXFile cxFile;
      clang_getFileLocation(location, &cxFile, &line, &column, &offset);
      cxsFile = clang_getFileName(cxFile);
      file = clang_getCString(cxsFile);
   }

   ~LocationDescription() {
      clang_disposeString(cxsFile);
   }

private:
   CXString cxsFile;
};

#define START_JSON \
   do { \
      bool is_first_json_field = true; \
      cout << "{ "; \
      /*LocationDescription location(cursor); \
      JSON_STRING("file", location.file) \
      JSON_STRING("line", location.line) \*/
      
#define JSON_VALUE(k, v) \
      if (is_first_json_field) { \
         cout << "\"" << k << "\": " << v; \
         is_first_json_field = false; \
      } else { \
         cout << ", \"" << k << "\": " << v; \
      }

#define JSON_STRING(k, v) \
      JSON_VALUE(k, "\"" << v << "\"")

#define END_JSON \
      cout << " }" << endl; \
   } while (0);

string to_s_and_dispose(CXString cxString);
CXChildVisitResult visit(CXCursor cursor, CXCursor parent, CXClientData data);
CXChildVisitResult visitMacroDefinition(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitNamespace(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitFunctionDecl(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitTypeDecl(string kind, CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitClassTemplate(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitUnionDecl(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitEnumDecl(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitEnumConstantDecl(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitClassDecl(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitStructDecl(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitCXXBaseSpecifier(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitCXXMethod(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitParmDecl(CXCursor cursor, CXCursor parent, Context* context);
CXChildVisitResult visitFieldDecl(CXCursor cursor, CXCursor parent, Context* context);

string to_s_and_dispose(CXString cxString) {
   string result = clang_getCString(cxString);
   clang_disposeString(cxString);
   return result;
}

string getCursorUSRString(CXCursor cursor) {
   return to_s_and_dispose(clang_getCursorUSR(cursor));
}

CXChildVisitResult visitMacroDefinition(CXCursor cursor, CXCursor parent, Context* context) {
   string macro = to_s_and_dispose(clang_getCursorSpelling(cursor));

   START_JSON
      JSON_STRING("kind", "MacroDefinition")
      JSON_STRING("name", macro)
   END_JSON

   return CXChildVisitResult::CXChildVisit_Recurse;
}

CXChildVisitResult visitNamespace(CXCursor cursor, CXCursor parent, Context* context) {
   string usr = getCursorUSRString(cursor);

   // TODO: Emit namespace object in json
   context->nested_namespaces.push_back(usr);
   clang_visitChildren(cursor, visit, context);
   context->nested_namespaces.pop_back();

   return CXChildVisitResult::CXChildVisit_Continue;
}

CXChildVisitResult visitFunctionDecl(CXCursor cursor, CXCursor parent, Context* context) {
   string name = to_s_and_dispose(clang_getCursorSpelling(cursor));
   string return_type = to_s_and_dispose(clang_getTypeSpelling(clang_getCursorResultType(cursor)));
   string usr = getCursorUSRString(cursor);

   START_JSON
      JSON_STRING("kind", "FunctionDecl")
      JSON_STRING("name", name)
      JSON_STRING("return_type", return_type)
      JSON_STRING("namespace", context->current_namespace())
      JSON_STRING("usr", usr)
   END_JSON

   return CXChildVisitResult::CXChildVisit_Recurse;
}

CXChildVisitResult visitTypeDecl(string kind, CXCursor cursor, CXCursor parent, Context* context) {
   string type_name;
   if (cursor.kind == CXCursorKind::CXCursor_ClassTemplate) {
      // The type spelling is blank for templates, since they aren't a real type yet (I guess).
      // 
      // The cursor spelling *usually* works for non-template types as well, except those that are anonymous.
      // For example, the following declaration would give an empty cursor spelling, but the correct type spelling:
      //  ``` 
      //  typedef struct { 
      //     int some_int;
      //   } my_struct;
      //  ```
      // So, prefer the type spelling which works for typedefs and regular type declarations.
      type_name = to_s_and_dispose(clang_getCursorSpelling(cursor));
   }
   else {
      type_name = to_s_and_dispose(clang_getTypeSpelling(clang_getCursorType(cursor)));
   }

   string usr = getCursorUSRString(cursor);

   START_JSON
      JSON_STRING("kind", kind)
      JSON_STRING("name", type_name)
      JSON_STRING("namespace", context->current_namespace())
      JSON_STRING("usr", usr)
   END_JSON

   context->nested_types.push_back(usr);
   clang_visitChildren(cursor, visit, context);
   context->nested_types.pop_back();

   return CXChildVisitResult::CXChildVisit_Continue;
}

CXChildVisitResult visitClassTemplate(CXCursor cursor, CXCursor parent, Context* context) {
   return visitTypeDecl("ClassTemplate", cursor, parent, context);
}

CXChildVisitResult visitUnionDecl(CXCursor cursor, CXCursor parent, Context* context) {
   return visitTypeDecl("UnionDecl", cursor, parent, context);
}

CXChildVisitResult visitEnumDecl(CXCursor cursor, CXCursor parent, Context* context) {
   return visitTypeDecl("EnumDecl", cursor, parent, context);
}

CXChildVisitResult visitEnumConstantDecl(CXCursor cursor, CXCursor parent, Context* context) {
   string name = to_s_and_dispose(clang_getCursorSpelling(cursor));
   long long value = clang_getEnumConstantDeclValue(cursor);
   string usr = getCursorUSRString(cursor);

   START_JSON
      JSON_STRING("kind", "EnumConstantDecl")
      JSON_STRING("name", name)
      JSON_VALUE("value", value)
      JSON_STRING("member_of", context->current_type())
      JSON_STRING("usr", usr)
   END_JSON

   return CXChildVisitResult::CXChildVisit_Recurse;
}

CXChildVisitResult visitClassDecl(CXCursor cursor, CXCursor parent, Context* context) {
   return visitTypeDecl("ClassDecl", cursor, parent, context);
}

CXChildVisitResult visitStructDecl(CXCursor cursor, CXCursor parent, Context* context) {
   return visitTypeDecl("StructDecl", cursor, parent, context);
}

CXChildVisitResult visitCXXBaseSpecifier(CXCursor cursor, CXCursor parent, Context* context) {
   string type_name = to_s_and_dispose(clang_getTypeSpelling(clang_getCursorType(cursor)));
   string parent_usr = getCursorUSRString(clang_getTypeDeclaration(clang_getCursorType(cursor)));

   START_JSON
      JSON_STRING("kind", "CXXBaseSpecifier")
      JSON_STRING("name", type_name)
      JSON_STRING("parent_usr", parent_usr)
      JSON_STRING("child_usr", context->current_type())
   END_JSON

   return CXChildVisitResult::CXChildVisit_Recurse;
}

CXChildVisitResult visitCXXMethod(CXCursor cursor, CXCursor parent, Context* context) {
   string method_name = to_s_and_dispose(clang_getCursorSpelling(cursor));
   CX_CXXAccessSpecifier access = clang_getCXXAccessSpecifier(cursor);
   string return_type_name = to_s_and_dispose(clang_getTypeSpelling(clang_getCursorResultType(cursor)));
   string usr = getCursorUSRString(cursor);

   START_JSON
      JSON_STRING("kind", "CXXMethod")
      JSON_STRING("name", method_name)
      JSON_STRING("return_type", return_type_name)
      switch (access) {
      case CX_CXXAccessSpecifier::CX_CXXPrivate:
         JSON_STRING("access", "private")
         break;
      case CX_CXXAccessSpecifier::CX_CXXPublic:
         JSON_STRING("access", "public")
         break;
      case CX_CXXAccessSpecifier::CX_CXXProtected:
         JSON_STRING("access", "protected")
         break;
      }
      if (clang_CXXMethod_isVirtual(cursor)) {
         JSON_VALUE("is_virtual", "true")
      }
      else {
         JSON_VALUE("is_virtual", "false")
      }
      JSON_STRING("member_of", context->current_type())
      JSON_STRING("usr", usr)
   END_JSON

   return CXChildVisitResult::CXChildVisit_Recurse;
}

CXChildVisitResult visitParmDecl(CXCursor cursor, CXCursor parent, Context* context) {
   string param_name = to_s_and_dispose(clang_getCursorSpelling(cursor));
   CXType type = clang_getCursorType(cursor);
   string type_name = to_s_and_dispose(clang_getTypeSpelling(type));

   bool is_pointer = type.kind == CXTypeKind::CXType_Pointer;
   string pointee_type_name = "";
   if (is_pointer) {
      pointee_type_name = to_s_and_dispose(clang_getTypeSpelling(clang_getPointeeType(type)));
   }

   START_JSON
      JSON_STRING("kind", "ParmDecl")
      JSON_STRING("name", param_name)
      JSON_STRING("type", type_name)
      if (is_pointer) {
         JSON_VALUE("is_pointer", (is_pointer ? "true" : "false"));
         JSON_STRING("pointee_type", pointee_type_name)
      }
   END_JSON

   return CXChildVisitResult::CXChildVisit_Recurse;
}

CXChildVisitResult visitFieldDecl(CXCursor cursor, CXCursor parent, Context* context) {
   string field_name = to_s_and_dispose(clang_getCursorSpelling(cursor));
   string field_type_name = to_s_and_dispose(clang_getTypeSpelling(clang_getCursorType(cursor)));
   CX_CXXAccessSpecifier access = clang_getCXXAccessSpecifier(cursor);
   string usr = getCursorUSRString(cursor);

   START_JSON
      JSON_STRING("kind", "FieldDecl")
      JSON_STRING("name", field_name)
      JSON_STRING("type", field_type_name)
      switch (access) {
      case CX_CXXAccessSpecifier::CX_CXXPrivate:
         JSON_STRING("access", "private")
            break;
      case CX_CXXAccessSpecifier::CX_CXXPublic:
         JSON_STRING("access", "public")
            break;
      case CX_CXXAccessSpecifier::CX_CXXProtected:
         JSON_STRING("access", "protected")
            break;
      }
      JSON_STRING("member_of", context->current_type())
      JSON_STRING("usr", usr)
   END_JSON

   return CXChildVisitResult::CXChildVisit_Recurse;
}

CXChildVisitResult visit(CXCursor cursor, CXCursor parent, CXClientData data) {
   if (!clang_Location_isFromMainFile(clang_getCursorLocation(cursor))) {
      return CXChildVisitResult::CXChildVisit_Continue;
   }

   Context* context = (Context*)data;

   CXChildVisitResult result = CXChildVisitResult::CXChildVisit_Recurse;

   switch (cursor.kind) {
   case CXCursorKind::CXCursor_MacroDefinition:
      result = visitMacroDefinition(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_Namespace:
      result = visitNamespace(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_ClassTemplate:
      result = visitClassTemplate(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_UnionDecl:
      result = visitUnionDecl(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_EnumDecl:
      result = visitEnumDecl(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_EnumConstantDecl:
      result = visitEnumConstantDecl(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_ClassDecl:
      result = visitClassDecl(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_StructDecl:
      result = visitStructDecl(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_CXXBaseSpecifier:
      result = visitCXXBaseSpecifier(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_FieldDecl:
      result = visitFieldDecl(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_CXXMethod:
      result = visitCXXMethod(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_FunctionDecl:
      result = visitFunctionDecl(cursor, parent, context);
      break;
   case CXCursorKind::CXCursor_ParmDecl:
      result = visitParmDecl(cursor, parent, context);
      break;
   }

   return result;
}

int main(int argc, char** argv) {
   Context context;
   CXIndex index = clang_createIndex(0, 0);

   context.translation_unit = clang_parseTranslationUnit(index, 0, argv, argc, 0, 0, CXTranslationUnit_DetailedPreprocessingRecord);

   if (NULL == context.translation_unit) {
      cout << "Error parsing translation unit." << endl;
   }

   CXCursor cursor = clang_getTranslationUnitCursor(context.translation_unit);

   clang_visitChildren(cursor, visit, &context);

   clang_disposeTranslationUnit(context.translation_unit);
   clang_disposeIndex(index);
}