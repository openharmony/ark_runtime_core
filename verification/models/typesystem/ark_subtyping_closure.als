/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

module ark_subtyping_closure

open ark_typesystem as ts

open util/ordering[Time]


sig Time {}

/*
premises:
1. type universe is static
2. table of type positions in type params too
3. dynamics is modelled only for type relation/subtyping
*/

one sig Viz {
  param_subtyping: Param -> Param -> Time,
  params_subtyping: Params -> Params -> Time,

  // show arcs of not closed subtyping relation
  not_closed : Type -> Type -> Time,

  // show arcs where subtyping closure contradicts subtyping by params
  forbidden: Type -> Type -> Time,

  // show arcs where sybtyping by params should be
  undertyped: Type -> Type -> Time
} {
  all t: Time
  | subtype_params_for_vizualization [
      param_subtyping.t,
      params_subtyping.t,
      TypeSystem.subtyping.t
  ]
  all t: Time | not_closed.t = ^(TypeSystem.subtyping.t) - TypeSystem.subtyping.t
  all t: Time | forbidden.t = TypeSystem.subtyping.t - 
                                       ts/all_subtypeable[TypeSystem.universe, TypeSystem.subtyping.t]
  all t: Time | undertyped.t = 
    ts/same_sort_arity_subtypeable[TypeSystem.universe, TypeSystem.subtyping.t] - TypeSystem.subtyping.t
}

one sig TypeSystem {
  universe: Sort -> Params -> Type,
  subtyping: Type -> Type -> Time,

  // table with positions of types in params of other types
  position_in_params_of_type: Type -> Int -> Type,

  // set of type pairs, that will be subtyped and added to
  // subtyping relation on next step
  to_be_subtyped: Type -> Type -> Time,
} {
  // calculation of table
  position_in_params_of_type = 
    { t1 : Type, i : Int, t2 : Type | t2.signature[i].type = t1}
}

// aux functions for readbility
fun sort[t: Type] : Sort { TypeSystem.universe.ts/sort[t] }
fun params[t: Type] : Params { TypeSystem.universe.ts/params[t] }
fun signature[t: Type] : seq Param { TypeSystem.universe.ts/signature[t] }
pred is_correct[subtyping: Type -> Type] { TypeSystem.universe.ts/is_correct[subtyping] }

// t1 <: t2 or t2 <: t1
pred related[t: Time, t1, t2 :Type] { some (t1 -> t2 + t2 -> t1) & TypeSystem.subtyping.t }

// return pairs of types when second type depends on first
let dep_types = {t1, t2: Type | t1 in TypeSystem.position_in_params_of_type.t2.Int}

pred allowed_to_be_related[t: Time, t1, t2 : Type] {
 let subtyping = ^(TypeSystem.subtyping.t + t1 -> t2)
 | subtyping in ts/all_subtypeable[TypeSystem.universe, subtyping]
 // for now it is explicetely stated
 no Viz.forbidden.last
}

// <> (t1 <: t2)
pred may_be_subtyped[t: Time, t1, t2: Type, subtyping: Type->Type] {
  t1.sort = t2.sort
  subtype[t1.params, t2.params, subtyping]
}

/*
Brief description of an idea:

let t1,t2 are types that are added in subtyping relation (t1<:t2), then
they can influence subtyping of other types only if:

1. t1,t2 are in same position of parameters

2. and parameters are of the same sort

3. and arity are the same too

Id est, only next forms of other types should be considered:

other_t1 = sort1(x,x, +t1,x,x,x)
other_t2 = sort1(x,x, +t2,x,x,x)

after calcualting set of affected type pairs, we relate t1 and t2
and repeat algorithm for each pair of types in calculated set.

after several steps, algorithm should converge and subtyping
relation will be properly closed
*/

fun common_indices[t1, t2: Type] : set Int {
  let pos = TypeSystem.position_in_params_of_type
  | let inds1 = t1.pos.Type, inds2 = t2.pos.Type
  | inds1 & inds2
}

fun complement_to_full[closure : Type->Type, addon : Type->Type] : Type -> Type {
  ^(closure + addon) - closure
}

fun affected[t: Time, ts : Type -> Type] : Type->Type {
  let pos = TypeSystem.position_in_params_of_type // alias for readability
  | {tn1, tn2: Type | // search tn1, tn2, that should be subtyped tn1 <: tn2 on next step
      {
      // they are not related yet
      not t.related[tn1, tn2]
      some t1, t2 : Type { // there are some t1 and t2, such that
         some (t1 -> t2 + t2 -> t1)  & ts // t1 and t2 were related in prev step
         let common = common_indices[t1, t2] {
              some common
              let ts1 = t1.pos[common], // take these types, where t1, t2 have common indices
                   ts2 = t2.pos[common] {
                // tn1, tn2 in ts1 x ts2
                tn1 in ts1
                tn2 in ts2
                // here we assume, that ts already in current subtyping
                t.may_be_subtyped[tn1, tn2, TypeSystem.subtyping.t + ts]
             }
         }
       }
      }
    }
}

// calculation of set of type pairs, that should be related due
// to relation of type pairs in ts
fun all_affected[t: Time, ts : Type -> Type] : Type->Type {
  let initially_affected = t.affected[ts]
  | complement_to_full[TypeSystem.subtyping.t, initially_affected]
}

// main step of algorithm
pred step[t: Time] {
  let prev = t.prev {
     // calculate next set of affected type pairs
     // strore this set of pairs for future step
     TypeSystem.to_be_subtyped.t = prev.all_affected[TypeSystem.to_be_subtyped.prev]
     // add previously related type pairs to previous subtyping relation to
     // form current subtyping relation
     TypeSystem.subtyping.t = TypeSystem.subtyping.prev + TypeSystem.to_be_subtyped.prev
  }
}

assert model_is_correct {
  {
    // at initial state type universe should be correct
    TypeSystem.universe.is_correct
    // initially we have correct subtyping relation
    TypeSystem.subtyping.first.is_correct

    // initially take any pair of unrelated types
    some t1, t2 : Type {
      not first.related[t1,t2]
      first.allowed_to_be_related[t1, t2]
      TypeSystem.to_be_subtyped.first = complement_to_full[TypeSystem.subtyping.first, t1 -> t2]
    }

    // run N steps of closure algorithm
    all t: Time - first | t.step
  } implies {
    // and finally we should have correct subtyping relation
    TypeSystem.subtyping.last.is_correct
    // algorithm terminate at last step
    no TypeSystem.to_be_subtyped.last
  }
// note, that algorithm has not to keep subtyping relation correct during its work
}

// max len of params of sorts (3 seq ) is sufficient to check all possible combinations
// of params variances, and helps to keep model search space reasonable sized
correctness: check model_is_correct for 4 but 10 Time

show: run {
  TypeSystem.universe.is_correct
  TypeSystem.subtyping.first.is_correct
  some t1, t2 : Type {
    not first.related[t1,t2]
    first.allowed_to_be_related[t1, t2]
    TypeSystem.to_be_subtyped.first = complement_to_full[TypeSystem.subtyping.first, t1 -> t2]
  }
  all t: Time - first | t.step
  some TypeSystem.to_be_subtyped.(first.next)
  #TypeSystem.to_be_subtyped.(first.next.next) > 1
  #TypeSystem.to_be_subtyped.(first.next.next.next) > 1
  #universe > 3
  #Params > 3
  #Sort > 2
} for 7

/*
Result:

-----------------------------------------------------------------------------------------------
Starting the solver...

   Forced to be exact: ts/Variance
   Forced to be exact: this/Time
Executing "Check correctness for 4 but 10 Time"
   Sig this/Time scope <= 10
   Sig this/Viz scope <= 1
   Sig this/TypeSystem scope <= 1
   Sig ts/Covariant scope <= 1
   Sig ts/Contrvariant scope <= 1
   Sig ts/Invariant scope <= 1
   Sig ts/TestTypeSystem scope <= 1
   Sig ts/ordering/Ord scope <= 1
   Sig ordering/Ord scope <= 1
   Sig ts/Variance scope <= 3
   Sig ts/Sort scope <= 4
   Sig ts/Type scope <= 4
   Sig ts/Param scope <= 4
   Sig ts/Params scope <= 4
   Sig this/Time forced to have exactly 10 atoms.
   Sig ts/Variance forced to have exactly 3 atoms.
   Field ts/ordering/Ord.First == [[ts/ordering/Ord$0, ts/Covariant$0]]
   Field ts/ordering/Ord.Next == [[ts/ordering/Ord$0, ts/Covariant$0, ts/Contrvariant$0], [ts/ordering/Ord$0, ts/Contrvariant$0, ts/Invariant$0]]
   Sig this/Time == [[Time$0], [Time$1], [Time$2], [Time$3], [Time$4], [Time$5], [Time$6], [Time$7], [Time$8], [Time$9]]
   Sig this/Viz == [[Viz$0]]
   Sig this/TypeSystem == [[TypeSystem$0]]
   Sig ts/Sort in [[ts/Sort$0], [ts/Sort$1], [ts/Sort$2], [ts/Sort$3]]
   Sig ts/Type in [[ts/Type$0], [ts/Type$1], [ts/Type$2], [ts/Type$3]]
   Sig ts/Variance == [[ts/Covariant$0], [ts/Contrvariant$0], [ts/Invariant$0]]
   Sig ts/Covariant == [[ts/Covariant$0]]
   Sig ts/Contrvariant == [[ts/Contrvariant$0]]
   Sig ts/Invariant == [[ts/Invariant$0]]
   Sig ts/Param in [[ts/Param$0], [ts/Param$1], [ts/Param$2], [ts/Param$3]]
   Sig ts/Params in [[ts/Params$0], [ts/Params$1], [ts/Params$2], [ts/Params$3]]
   Sig ts/TestTypeSystem == [[ts/TestTypeSystem$0]]
   Sig ts/ordering/Ord == [[ts/ordering/Ord$0]]
   Sig ordering/Ord == [[ordering/Ord$0]]
   Generating facts...
   Simplifying the bounds...
   Solver=minisat(jni) Bitwidth=4 MaxSeq=4 SkolemDepth=4 Symmetry=20
   Generating CNF...
   Generating the solution...
   78472 vars. 1668 primary vars. 254676 clauses. 1117ms.
   No counterexample found. Assertion may be valid. 394370ms.
-----------------------------------------------------------------------------------------------

Conclusion:
1. Model sizes are sufficient to find algorithm errors
2. No errors are found (modulo specific selection of initial typesystems, see comments above)
3. Algorithm implements correct subtyping closure
4. Algorithm is terminating

*/
