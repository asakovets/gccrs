// Copyright (C) 2021 Free Software Foundation, Inc.

// This file is part of GCC.

// GCC is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3, or (at your option) any later
// version.

// GCC is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.

// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#ifndef RUST_HIR_TRAIT_REF_H
#define RUST_HIR_TRAIT_REF_H

#include "rust-hir-full.h"
#include "rust-tyty-visitor.h"

namespace Rust {
namespace Resolver {

// Data Objects for the associated trait items in a structure we can work with
// https://doc.rust-lang.org/edition-guide/rust-2018/trait-system/associated-constants.html
class TypeCheckContext;
class TraitItemReference
{
public:
  enum TraitItemType
  {
    FN,
    CONST,
    TYPE,
    ERROR
  };

  TraitItemReference (std::string identifier, bool optional, TraitItemType type,
		      HIR::TraitItem *hir_trait_item, TyTy::BaseType *self,
		      std::vector<TyTy::SubstitutionParamMapping> substitutions,
		      Location locus);

  TraitItemReference (TraitItemReference const &other);

  TraitItemReference &operator= (TraitItemReference const &other)
  {
    identifier = other.identifier;
    optional_flag = other.optional_flag;
    type = other.type;
    hir_trait_item = other.hir_trait_item;
    self = other.self;
    locus = other.locus;
    context = other.context;

    inherited_substitutions.reserve (other.inherited_substitutions.size ());
    for (size_t i = 0; i < other.inherited_substitutions.size (); i++)
      inherited_substitutions.push_back (other.inherited_substitutions.at (i));

    return *this;
  }

  TraitItemReference (TraitItemReference &&other) = default;
  TraitItemReference &operator= (TraitItemReference &&other) = default;

  static TraitItemReference error ()
  {
    return TraitItemReference ("", false, ERROR, nullptr, nullptr, {},
			       Location ());
  }

  static TraitItemReference &error_node ()
  {
    static TraitItemReference error = TraitItemReference::error ();
    return error;
  }

  bool is_error () const { return type == ERROR; }

  std::string as_string () const
  {
    return "(" + trait_item_type_as_string (type) + " " + identifier + " "
	   + ")";
  }

  static std::string trait_item_type_as_string (TraitItemType ty)
  {
    switch (ty)
      {
      case FN:
	return "FN";
      case CONST:
	return "CONST";
      case TYPE:
	return "TYPE";
      case ERROR:
	return "ERROR";
      }
    return "ERROR";
  }

  bool is_optional () const { return optional_flag; }

  std::string get_identifier () const { return identifier; }

  TraitItemType get_trait_item_type () const { return type; }

  const HIR::TraitItem *get_hir_trait_item () const { return hir_trait_item; }

  Location get_locus () const { return locus; }

  const Analysis::NodeMapping &get_mappings () const
  {
    return hir_trait_item->get_mappings ();
  }

  TyTy::BaseType *get_tyty () const
  {
    rust_assert (hir_trait_item != nullptr);

    switch (type)
      {
      case CONST:
	return get_type_from_constant (
	  static_cast</*const*/ HIR::TraitItemConst &> (*hir_trait_item));
	break;

      case TYPE:
	return get_type_from_typealias (
	  static_cast</*const*/ HIR::TraitItemType &> (*hir_trait_item));

      case FN:
	return get_type_from_fn (
	  static_cast</*const*/ HIR::TraitItemFunc &> (*hir_trait_item));
	break;

      default:
	return get_error ();
      }

    gcc_unreachable ();
    return get_error ();
  }

private:
  TyTy::ErrorType *get_error () const
  {
    return new TyTy::ErrorType (get_mappings ().get_hirid ());
  }

  TyTy::BaseType *get_type_from_typealias (/*const*/
					   HIR::TraitItemType &type) const;

  TyTy::BaseType *
  get_type_from_constant (/*const*/ HIR::TraitItemConst &constant) const;

  TyTy::BaseType *get_type_from_fn (/*const*/ HIR::TraitItemFunc &fn) const;

  std::string identifier;
  bool optional_flag;
  TraitItemType type;
  HIR::TraitItem *hir_trait_item;
  std::vector<TyTy::SubstitutionParamMapping> inherited_substitutions;
  Location locus;

  TyTy::BaseType
    *self; // this is the implict Self TypeParam required for methods
  Resolver::TypeCheckContext *context;
};

class TraitReference
{
public:
  TraitReference (const HIR::Trait *hir_trait_ref,
		  std::vector<TraitItemReference> item_refs)
    : hir_trait_ref (hir_trait_ref), item_refs (item_refs)
  {}

  TraitReference (TraitReference const &other)
    : hir_trait_ref (other.hir_trait_ref), item_refs (other.item_refs)
  {}

  TraitReference &operator= (TraitReference const &other)
  {
    hir_trait_ref = other.hir_trait_ref;
    item_refs = other.item_refs;

    return *this;
  }

  TraitReference (TraitReference &&other) = default;
  TraitReference &operator= (TraitReference &&other) = default;

  static TraitReference error () { return TraitReference (nullptr, {}); }

  bool is_error () const { return hir_trait_ref == nullptr; }

  Location get_locus () const { return hir_trait_ref->get_locus (); }

  std::string get_name () const
  {
    rust_assert (!is_error ());
    return hir_trait_ref->get_name ();
  }

  std::string as_string () const
  {
    if (is_error ())
      return "<trait-ref-error-node>";

    std::string item_buf;
    for (auto &item : item_refs)
      {
	item_buf += item.as_string () + ", ";
      }
    return "HIR Trait: " + get_name () + "->"
	   + hir_trait_ref->get_mappings ().as_string () + " [" + item_buf
	   + "]";
  }

  const TraitItemReference &lookup_trait_item (const std::string &ident) const
  {
    for (auto &item : item_refs)
      {
	if (ident.compare (item.get_identifier ()) == 0)
	  return item;
      }
    return TraitItemReference::error_node ();
  }

  const TraitItemReference &
  lookup_trait_item (const std::string &ident,
		     TraitItemReference::TraitItemType type) const
  {
    for (auto &item : item_refs)
      {
	if (item.get_trait_item_type () != type)
	  continue;

	if (ident.compare (item.get_identifier ()) == 0)
	  return item;
      }
    return TraitItemReference::error_node ();
  }

  size_t size () const { return item_refs.size (); }

  const std::vector<TraitItemReference> &get_trait_items () const
  {
    return item_refs;
  }

private:
  const HIR::Trait *hir_trait_ref;
  std::vector<TraitItemReference> item_refs;
};

} // namespace Resolver
} // namespace Rust

#endif // RUST_HIR_TRAIT_REF_H
