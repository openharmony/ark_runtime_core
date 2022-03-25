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

module check_set_intersection_as_lub
open java_typing

fact Validity {all r: Register | r.Valid }
let supertypes[t] = { t.^~subtypes }

fun SuperTypesIntersection[regs: set Register] : set Type 
{{t : Type | all r : regs | t in r.type + r.type.supertypes}}

-- Correct subtyping definition: 
-- exists some type in set of types,
-- which is a subtype of register type
pred CorrectSubtyping[r: Register, types: set Type]
{ some t : types | t in (r.type + r.type.^subtypes) & ProperType }

-- check that one reg type may be used as substitutuion for the other
pred MayBeSubstitutedFor[subst, r:Register]
{ subst.type in (r.type + r.type.subtypes) }

-- r1,r2,r3 - reg values accured during context calculation
-- arg - is some type to be checked for compatibility with context
-- like some argument type in call instruction, etc
-- compatibility means that either of r1,r2,r3 may be used for arg
pred Show  {
  Register.holds in Instance
  some Class
  some Interface
  some Register.type & Interface
  some disj r1, r2, r3, arg : Register {
    let comp_types = SuperTypesIntersection[r1+r2+r3] {
      some comp_types - Top and arg.CorrectSubtyping[comp_types] and {
        r1.MayBeSubstitutedFor[arg]
        r2.MayBeSubstitutedFor[arg]
        r3.MayBeSubstitutedFor[arg]
      }
    }
  }
}

example: run Show for 7 but exactly 4 Register, exactly 5 Class, exactly 5 Interface, exactly 3 Instance

assert SupertypesIntersectionAsLUB {
  {
    #Register >=4
    #Class >= 1
    #Instance >= 1
  } implies 
  some r1, r2, r3, arg : Register {
  -- select some regs (suppose it is the same reg from different contexts)
  -- select target arg, that will be checked against merged context
    let comp_types = SuperTypesIntersection[r1+r2+r3] {
    -- calculate supertypes intersection on contexts merge
      {
         some comp_types - Top  -- we have some proper types after contexts merge
         arg.CorrectSubtyping[comp_types] -- and arg is checked against this new context
      } implies {
        -- so verify, that all reg instances from all contexts can be used for arg
        r1.MayBeSubstitutedFor[arg]
        r2.MayBeSubstitutedFor[arg]
        r3.MayBeSubstitutedFor[arg]
      }
    }
  }
}

verify: check SupertypesIntersectionAsLUB for 8

-- results:
--
-- Executing "Check verify for 8"
--    Sig java_typing/Top scope <= 1
--    Sig java_typing/Bot scope <= 1
--    Sig java_typing/Object scope <= 1
--    Sig java_typing/NullType scope <= 1
--    Sig java_typing/Null scope <= 1
--    Sig java_typing/Type scope <= 8
--    Sig java_typing/Value scope <= 8
--    Sig java_typing/Register scope <= 8
--    Sig java_typing/Instance scope <= 7
--    Sig java_typing/Class scope <= 8
--    Sig java_typing/Interface scope <= 8
--    Sig java_typing/Type in [[java_typing/Top$0], [java_typing/Bot$0], [java_typing/Object$0], [java_typing/NullType$0], [java_typing/Type$0], [java_typing/Type$1], [java_typing/Type$2], [java_typing/Type$3]]
--    Sig java_typing/Top == [[java_typing/Top$0]]
--    Sig java_typing/Bot == [[java_typing/Bot$0]]
--    Sig java_typing/Class in [[java_typing/Type$0], [java_typing/Type$1], [java_typing/Type$2], [java_typing/Type$3]]
--    Sig java_typing/Interface in [[java_typing/Type$0], [java_typing/Type$1], [java_typing/Type$2], [java_typing/Type$3]]
--    Sig java_typing/Object == [[java_typing/Object$0]]
--    Sig java_typing/NullType == [[java_typing/NullType$0]]
--    Sig java_typing/Value in [[java_typing/Null$0], [java_typing/Value$0], [java_typing/Value$1], [java_typing/Value$2], [java_typing/Value$3], [java_typing/Value$4], [java_typing/Value$5], [java_typing/Value$6]]
--    Sig java_typing/Instance in [[java_typing/Value$0], [java_typing/Value$1], [java_typing/Value$2], [java_typing/Value$3], [java_typing/Value$4], [java_typing/Value$5], [java_typing/Value$6]]
--    Sig java_typing/Null == [[java_typing/Null$0]]
--    Sig java_typing/Register in [[java_typing/Register$0], [java_typing/Register$1], [java_typing/Register$2], [java_typing/Register$3], [java_typing/Register$4], [java_typing/Register$5], [java_typing/Register$6], [java_typing/Register$7]]
--    Generating facts...
--    Simplifying the bounds...
--    Solver=sat4j Bitwidth=4 MaxSeq=7 SkolemDepth=4 Symmetry=20
--    Generating CNF...
--    Generating the solution...
--    502376 vars. 33023 primary vars. 881131 clauses. 3609ms.
--    No counterexample found. Assertion may be valid. 127ms.
