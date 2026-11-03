/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "libe3", "index.html", [
    [ "libe3 - Vendor-Neutral E3AP C++ Library", "index.html", "index" ],
    [ "Path A: the E3-only end-to-end loop", "md_docs_2control-loops_2path-a-e3-loop.html", [
      [ "Box order", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md9", null ],
      [ "Forward leg: RAN to dApp (<span class=\"tt\">leg=ind_up</span>)", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md10", [
        [ "Caveat on A6 and the wire", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md11", null ]
      ] ],
      [ "Return leg: dApp to RAN (<span class=\"tt\">leg=ctrl_down</span> for control, <span class=\"tt\">report_up</span> for reports)", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md12", [
        [ "Two distinct terminations", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md13", null ]
      ] ],
      [ "Aggregates", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md14", null ],
      [ "Deployment variants", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md15", [
        [ "Comparing against a hand-rolled integration", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md16", null ]
      ] ],
      [ "Not yet instrumented", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md17", null ]
    ] ],
    [ "Path B: the full E2-E3 loop", "md_docs_2control-loops_2path-b-e2-e3-loop.html", [
      [ "Box order", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md19", null ],
      [ "The two RIC profiles", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md20", null ],
      [ "Report-up leg (<span class=\"tt\">leg=report_up</span>)", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md21", null ],
      [ "Policy-down leg (<span class=\"tt\">leg=policy_down</span>)", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md22", null ],
      [ "Mitigation leg (<span class=\"tt\">leg=mitigation</span>)", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md23", null ],
      [ "Aggregates", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md24", null ],
      [ "Joining: the <span class=\"tt\">sequenceId</span>", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md25", [
        [ "Where it shows up in the rings", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md26", null ]
      ] ],
      [ "Not yet instrumented", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md27", null ]
    ] ],
    [ "E2SM-DAPP: identity and registration", "md_docs_2e2sm-dapp_201-identity-and-registration.html", [
      [ "1. Identity", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md29", [
        [ "1.1 How the OID is built", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md30", null ],
        [ "1.2 Two things named \"RAN function ID\"", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md31", null ]
      ] ],
      [ "2. RAN function definition: what it contains", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md32", [
        [ "2.1 What a RAN sends", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md33", null ],
        [ "2.2 Who sets the name block", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md34", null ]
      ] ],
      [ "3. Announcement in E2 Setup and RIC Service Update", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md35", [
        [ "3.1 E2 Setup Request", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md36", null ],
        [ "3.2 RIC Service Update", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md37", null ]
      ] ],
      [ "4. Requirements for a RAN that announces E2SM-DAPP", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md38", null ]
    ] ],
    [ "E2SM-DAPP: E2AP information element mapping", "md_docs_2e2sm-dapp_202-ie-mapping.html", [
      [ "1. The elements at a glance", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md40", null ],
      [ "2. Which E2 service uses which format", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md41", [
        [ "2.1 Why INSERT, POLICY and QUERY are \"not used\"", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md42", null ],
        [ "2.2 Why there is no call process ID", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md43", null ]
      ] ],
      [ "3. Common types", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md44", null ],
      [ "4. Event trigger and action definition", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md45", [
        [ "4.1 <span class=\"tt\">E2SM-DAPP-EventTrigger</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md46", null ],
        [ "4.2 <span class=\"tt\">E2SM-DAPP-ActionDefinition</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md47", null ]
      ] ],
      [ "5. RAN function definition", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md48", [
        [ "5.1 <span class=\"tt\">RANfunction-Name</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md49", null ],
        [ "5.2 <span class=\"tt\">RANFunctionDefinition-Report</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md50", null ],
        [ "5.3 <span class=\"tt\">RANFunctionDefinition-Control</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md51", null ],
        [ "5.4 dApp E3 subscription list", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md52", null ]
      ] ],
      [ "6. Indication header and message", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md53", [
        [ "6.1 <span class=\"tt\">E2SM-DAPP-IndicationHeader</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md54", null ],
        [ "6.2 <span class=\"tt\">E2SM-DAPP-IndicationMessage</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md55", null ]
      ] ],
      [ "7. Control header, message, outcome", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md56", [
        [ "7.1 <span class=\"tt\">E2SM-DAPP-ControlHeader</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md57", null ],
        [ "7.2 <span class=\"tt\">E2SM-DAPP-ControlMessage</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md58", null ],
        [ "7.3 <span class=\"tt\">E2SM-DAPP-ControlOutcome</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md59", null ]
      ] ],
      [ "8. The <span class=\"tt\">e3-control-outcome</span> payload", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md60", [
        [ "8.1 Who owns what", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md61", null ],
        [ "8.2 Worked example: the Spectrum service model", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md62", null ],
        [ "8.3 How a consumer reads it", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md63", null ],
        [ "8.4 How another service model adds its own outcome", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md64", null ]
      ] ],
      [ "9. Where the elements are produced and consumed", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md65", null ],
      [ "10. Other representations of the elements", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md66", null ]
    ] ],
    [ "E2SM-DAPP: procedure flow", "md_docs_2e2sm-dapp_203-procedure-flow.html", [
      [ "Roles", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md68", null ],
      [ "Messages on the flow", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md69", null ],
      [ "Discovery: how an xApp learns the dApp ids", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md70", [
        [ "In the RAN function definition", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md71", null ],
        [ "When the set of dApps changes", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md72", null ],
        [ "In every report header", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md73", null ],
        [ "What discovery does not do", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md74", null ]
      ] ],
      [ "Subscription: pairing the xApp with the report stream", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md75", null ],
      [ "Indication delivery (report up)", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md76", null ],
      [ "Control relay and the deferred acknowledge", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md77", [
        [ "Policy down: from the xApp to the dApp", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md78", null ],
        [ "Mitigation: from the dApp back to the air", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md79", null ],
        [ "The E2 acknowledge is deferred to B19", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md80", null ],
        [ "The mitigation figure", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md81", null ]
      ] ],
      [ "The loop on the boxes", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md82", [
        [ "The sequence-id chain", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md83", null ],
        [ "The same flow as a sequence", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md84", null ]
      ] ],
      [ "Differences between implementations", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md85", null ]
    ] ],
    [ "E2SM-DAPP: encoding", "md_docs_2e2sm-dapp_204-encoding.html", [
      [ "What is on the wire", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md87", [
        [ "The three rules that decide almost every byte", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md88", null ],
        [ "Encoding rules, by ASN.1 construct", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md89", null ],
        [ "Byte order, packing, length prefixes", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md90", null ],
        [ "A receiver that sees extension additions", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md91", null ]
      ] ],
      [ "Maximum sizes", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md92", null ],
      [ "Worked examples", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md93", [
        [ "<span class=\"tt\">et_f1</span> = <span class=\"tt\">00</span> (event trigger)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md94", null ],
        [ "<span class=\"tt\">ad_style1</span> = <span class=\"tt\">00 01 01 00</span> (action definition, report style 1)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md95", null ],
        [ "<span class=\"tt\">ih1_min</span> = <span class=\"tt\">00 00 ff 00 01 00 00 13 00 14 00 00</span> (indication header format 1, only mandatory fields)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md96", null ],
        [ "<span class=\"tt\">ih1_full</span> = <span class=\"tt\">0e 00 ff 00 07 00 02 00 f1 10 20 04 d2 00 05 08 17 97 9c fe 3d 85 cd 15 01 2a</span>", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md97", null ],
        [ "<span class=\"tt\">ch1_min</span> = <span class=\"tt\">00 01 00 02</span> (control header, no timestamp, no sequence-id)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md98", null ],
        [ "<span class=\"tt\">co1_full</span> = <span class=\"tt\">1c 08 17979cfe3d85cd15 01 2a 0a 10 20 30 40 50 60 70 80 90 a0</span> (control outcome, everything present)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md99", null ],
        [ "Two APER layers nest in the control outcome", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md100", null ],
        [ "<span class=\"tt\">im1_small</span> = <span class=\"tt\">00 00 04 00 03 de ad be ef</span> (indication message format 1, 4 octets of payload)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md101", null ],
        [ "<span class=\"tt\">im2_one</span> = <span class=\"tt\">20 00 01 00 01 00 00 01</span> (indication message format 2, one dApp, one function)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md102", null ],
        [ "<span class=\"tt\">fd_min</span>, the smallest RAN function definition", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md103", null ]
      ] ],
      [ "Reference test vectors (full list)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md104", null ],
      [ "Native types for the grammar types", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md105", null ],
      [ "Checking an encoding", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md106", null ],
      [ "Abstract syntax", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md107", null ]
    ] ],
    [ "E2SM-DAPP: semantics and constraints", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html", [
      [ "1. Numbering: formats and styles", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md109", null ],
      [ "2. Identifiers", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md110", null ],
      [ "3. Clocks and units", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md111", [
        [ "Whose clock is each timestamp on", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md112", null ]
      ] ],
      [ "4. Optional fields: absent is not an error", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md113", null ],
      [ "5. What is mandatory", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md114", null ],
      [ "6. Validation and error handling", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md115", [
        [ "6.1 On encode", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md116", null ],
        [ "6.2 On decode", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md117", null ],
        [ "6.3 Error paths in summary", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md118", null ]
      ] ],
      [ "7. Behavior on unknown ids and formats", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md119", null ],
      [ "8. Control semantics", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md120", [
        [ "8.1 What \"applied\" means, and what the acknowledge says", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md121", null ],
        [ "8.2 What a consumer shall do", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md122", null ],
        [ "8.3 Ordering and replacement", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md123", null ]
      ] ],
      [ "9. Rules that cross elements", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md124", null ]
    ] ],
    [ "E2SM-DAPP: implementation notes and known discrepancies", "md_docs_2e2sm-dapp_206-discrepancies.html", [
      [ "1. Dead code", "md_docs_2e2sm-dapp_206-discrepancies.html#autotoc_md126", null ],
      [ "2. Where the grammar and the implementations disagree", "md_docs_2e2sm-dapp_206-discrepancies.html#autotoc_md127", null ],
      [ "3. Aborts and error handling in implementation A", "md_docs_2e2sm-dapp_206-discrepancies.html#autotoc_md128", null ],
      [ "4. Consumers", "md_docs_2e2sm-dapp_206-discrepancies.html#autotoc_md129", null ],
      [ "5. Differences between the implementations", "md_docs_2e2sm-dapp_206-discrepancies.html#autotoc_md130", [
        [ "5.1 Implementation C's E2 module against implementation A and the OpenAirInterface RAN", "md_docs_2e2sm-dapp_206-discrepancies.html#autotoc_md131", null ],
        [ "5.2 Runtime helpers of implementation C that the port works around", "md_docs_2e2sm-dapp_206-discrepancies.html#autotoc_md132", null ]
      ] ],
      [ "6. What the reference test vectors prove and what they do not", "md_docs_2e2sm-dapp_206-discrepancies.html#autotoc_md133", null ],
      [ "7. Open items", "md_docs_2e2sm-dapp_206-discrepancies.html#autotoc_md134", null ]
    ] ],
    [ "E2SM-DAPP: code pointers and evidence", "md_docs_2e2sm-dapp_2code-pointers.html", [
      [ "Chapter README: Artifacts and code roots", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md136", [
        [ "Where each part is specified", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md137", null ],
        [ "Where the artifacts are", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md138", null ],
        [ "Related documents", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md139", null ],
        [ "Where the code references point", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md140", null ]
      ] ],
      [ "Chapter 1: Identity and registration (code evidence)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md141", [
        [ "Identity values and where they are defined", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md142", null ],
        [ "The OID comment", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md143", null ],
        [ "Two kinds of \"RAN function ID\"", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md144", null ],
        [ "Where the identity is written down (section 1.3 of the old text)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md145", null ],
        [ "What a real RAN sends (old section 2.1)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md146", null ],
        [ "What flexric's agent adds or replaces (old section 2.2)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md147", null ],
        [ "E2 Setup Request (old section 3.1)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md148", null ],
        [ "RIC Service Update (old section 3.2)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md149", null ],
        [ "Checklist notes (old section 4)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md150", null ]
      ] ],
      [ "Chapter 2: E2AP information element mapping (code evidence)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md151", [
        [ "Style numbers, formats and names (old section 2, \"Where each fact comes from\")", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md152", null ],
        [ "Why INSERT, POLICY and QUERY are \"not used\"", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md153", null ],
        [ "Why there is no call process ID", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md154", null ],
        [ "Common types", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md155", null ],
        [ "Event trigger and action definition", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md156", null ],
        [ "RAN function definition", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md157", null ],
        [ "Indication header", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md158", null ],
        [ "Control header and outcome", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md159", null ],
        [ "The Spectrum outcome worked example (old section 8.2)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md160", null ],
        [ "How a consumer reads the outcome (old section 8.3)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md161", null ],
        [ "E3 RAN function id registry (old section 8.4)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md162", null ],
        [ "Where the elements are produced and consumed (old section 9)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md163", null ],
        [ "The same elements in libe3 and OCUDU (old section 10)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md164", null ]
      ] ],
      [ "Chapter 3: Procedure flow (code evidence moved out of 03-procedure-flow.md)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md165", [
        [ "The cast: who is who in the code", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md166", null ],
        [ "Every message on the flow", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md167", null ],
        [ "Discovery: the RAN function definition (E2 Setup and RIC Service Update)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md168", null ],
        [ "When the set of dApps changes", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md169", null ],
        [ "Discovery in every report header", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md170", null ],
        [ "What discovery does not do", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md171", null ],
        [ "Subscription", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md172", null ],
        [ "Indication delivery (report up)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md173", null ],
        [ "Control relay: policy down", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md174", null ],
        [ "Control relay: mitigation back to the air", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md175", null ],
        [ "The deferred acknowledge", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md176", null ],
        [ "The mitigation figure", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md177", null ],
        [ "The sequence-id chain", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md178", null ],
        [ "The same flow in each implementation", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md179", null ]
      ] ],
      [ "Chapter 4: Encoding (code evidence moved out of 04-encoding.md)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md180", [
        [ "What is on the wire", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md181", null ],
        [ "Byte order, packing, length prefixes", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md182", null ],
        [ "A receiver that sees extension additions", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md183", null ],
        [ "Implementation limits tighter than the grammar", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md184", null ],
        [ "Worked examples: where the bytes come from", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md185", null ],
        [ "Golden vectors (full list)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md186", null ],
        [ "The type map across implementations", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md187", [
          [ "Type names", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md188", null ],
          [ "How the same field is held", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md189", null ],
          [ "The struct layout of flexric is an ABI", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md190", null ]
        ] ],
        [ "Build switches", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md191", [
          [ "flexric: <span class=\"tt\">SM_ENCODING_DAPP</span>", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md192", null ],
          [ "libe3: <span class=\"tt\">LIBE3_ENABLE_E2SM_DAPP</span>", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md193", null ],
          [ "OCUDU: <span class=\"tt\">e2sm_dapp_enabled</span>", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md194", null ]
        ] ],
        [ "Reproducing and checking an encoding", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md195", null ]
      ] ],
      [ "Chapter 5: Semantics and constraints", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md196", [
        [ "1. Numbering: formats and styles", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md197", null ],
        [ "2. Identifiers", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md198", null ],
        [ "3. Clocks and units", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md199", null ],
        [ "Whose clock is each timestamp on", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md200", null ],
        [ "4. Optional fields: absent is not an error", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md201", null ],
        [ "5. What the code treats as mandatory", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md202", null ],
        [ "6. What each implementation checks", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md203", [
          [ "6.1 On encode", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md204", null ],
          [ "6.2 On decode", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md205", null ],
          [ "6.3 Error paths in summary", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md206", null ]
        ] ],
        [ "7. Behavior on unknown ids and formats", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md207", null ],
        [ "8. Control semantics", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md208", [
          [ "8.1 What \"applied\" means, and what the acknowledge says", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md209", null ],
          [ "8.2 What a consumer must do", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md210", null ],
          [ "8.3 Ordering and replacement", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md211", null ]
        ] ],
        [ "9. Rules that cross elements", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md212", null ]
      ] ],
      [ "Chapter 6: Discrepancies, TODOs and dead code", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md213", [
        [ "1. Dead code", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md214", null ],
        [ "2. Where the grammar and the code disagree", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md215", null ],
        [ "3. Aborts and error handling in flexric", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md216", null ],
        [ "4. Consumers", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md217", null ],
        [ "5. Differences between the implementations", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md218", [
          [ "5.1 OCUDU's E2 module against flexric and OAI", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md219", null ],
          [ "5.2 OCUDU runtime helpers the port works around", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md220", null ]
        ] ],
        [ "6. What the golden vectors prove and what they do not", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md221", null ],
        [ "7. Open items", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md222", null ]
      ] ]
    ] ],
    [ "E2SM-DAPP: integrator guide", "md_docs_2e2sm-dapp_2integrator-guide.html", [
      [ "Choose a route", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md224", null ],
      [ "Route A: link libe3", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md225", [
        [ "Build and install libe3", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md226", null ],
        [ "Use it from CMake", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md227", null ],
        [ "C++: pack an indication header and indication message", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md228", null ],
        [ "C++: unpack a control and read it", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md229", null ],
        [ "C++: build the control outcome (the RAN side of an acknowledge)", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md230", null ],
        [ "C: the same, from C", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md231", null ]
      ] ],
      [ "Route B: run asn1c yourself", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md232", [
        [ "The asn1c you must use", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md233", null ],
        [ "The exact flags", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md234", null ],
        [ "Compile definitions and layout", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md235", null ]
      ] ],
      [ "Route C: a hand-written codec (OCUDU)", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md236", null ],
      [ "The OCUDU to libe3 C++ interface", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md237", null ],
      [ "The contract: the golden vectors", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md238", [
        [ "The shared test-fixture technique", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md239", null ],
        [ "Reproducing the golden vectors", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md240", null ]
      ] ],
      [ "Checklist for a new stack", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md241", null ],
      [ "What is deliberately not here", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md242", null ]
    ] ],
    [ "E3AP: identity and registration", "md_docs_2e3ap_201-identity-and-registration.html", [
      [ "Identity", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md259", [
        [ "Two sides of one interface", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md260", null ],
        [ "Identifiers", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md261", null ]
      ] ],
      [ "Links and transports", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md262", [
        [ "The three channels", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md263", null ],
        [ "Default endpoints", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md264", null ]
      ] ],
      [ "How a dApp registers", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md265", [
        [ "What the RAN function list holds", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md266", null ]
      ] ],
      [ "Requirements for an E3 Agent", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md267", null ],
      [ "Related", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md268", null ]
    ] ],
    [ "E3AP: information elements", "md_docs_2e3ap_202-ie-mapping.html", [
      [ "The messages at a glance", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md270", null ],
      [ "Common types", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md271", null ],
      [ "E3-PDU", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md272", null ],
      [ "E3-SetupRequest", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md273", null ],
      [ "E3-SetupResponse", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md274", null ],
      [ "E3-RanFunctionDefinition", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md275", null ],
      [ "E3-SubscriptionRequest", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md276", null ],
      [ "E3-SubscriptionResponse", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md277", null ],
      [ "E3-SubscriptionDelete", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md278", null ],
      [ "E3-IndicationMessage", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md279", null ],
      [ "E3-DAppControlAction", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md280", null ],
      [ "E3-DAppReport", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md281", null ],
      [ "E3-XAppControlAction", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md282", null ],
      [ "E3-ReleaseMessage", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md283", null ],
      [ "E3-MessageAck", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md284", null ],
      [ "Presence notation", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md285", null ]
    ] ],
    [ "E3AP: procedure flow", "md_docs_2e3ap_203-procedure-flow.html", [
      [ "Roles", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md287", null ],
      [ "Messages on the flow", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md288", null ],
      [ "Setup", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md289", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md290", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md291", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md292", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md293", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md294", null ]
      ] ],
      [ "Subscription", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md295", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md296", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md297", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md298", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md299", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md300", null ]
      ] ],
      [ "Subscription delete", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md301", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md302", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md303", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md304", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md305", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md306", null ]
      ] ],
      [ "Indication", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md307", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md308", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md309", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md310", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md311", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md312", null ]
      ] ],
      [ "dApp Control", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md313", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md314", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md315", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md316", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md317", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md318", null ]
      ] ],
      [ "dApp Report", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md319", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md320", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md321", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md322", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md323", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md324", null ]
      ] ],
      [ "xApp Control", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md325", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md326", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md327", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md328", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md329", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md330", null ]
      ] ],
      [ "Release", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md331", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md332", null ],
        [ "Procedure from the dApp", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md333", null ],
        [ "Procedure from the E3 Agent", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md334", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md335", null ]
      ] ],
      [ "The sequence identifier chain", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md336", null ],
      [ "Threads and queues", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md337", null ]
    ] ],
    [ "E3AP: encoding", "md_docs_2e3ap_204-encoding.html", [
      [ "What is on the wire", "md_docs_2e3ap_204-encoding.html#autotoc_md339", null ],
      [ "APER", "md_docs_2e3ap_204-encoding.html#autotoc_md340", [
        [ "The rules that decide almost every byte", "md_docs_2e3ap_204-encoding.html#autotoc_md341", null ],
        [ "Rules for each construct", "md_docs_2e3ap_204-encoding.html#autotoc_md342", null ],
        [ "Worked examples", "md_docs_2e3ap_204-encoding.html#autotoc_md343", null ],
        [ "Size of the encoding", "md_docs_2e3ap_204-encoding.html#autotoc_md344", null ],
        [ "A receiver that sees extension additions", "md_docs_2e3ap_204-encoding.html#autotoc_md345", null ]
      ] ],
      [ "JSON", "md_docs_2e3ap_204-encoding.html#autotoc_md346", null ],
      [ "Protobuf", "md_docs_2e3ap_204-encoding.html#autotoc_md347", null ],
      [ "Framing on each link", "md_docs_2e3ap_204-encoding.html#autotoc_md348", [
        [ "ZeroMQ", "md_docs_2e3ap_204-encoding.html#autotoc_md349", null ],
        [ "POSIX sockets", "md_docs_2e3ap_204-encoding.html#autotoc_md350", null ]
      ] ],
      [ "Size limits", "md_docs_2e3ap_204-encoding.html#autotoc_md351", null ],
      [ "Abstract syntax", "md_docs_2e3ap_204-encoding.html#autotoc_md352", null ]
    ] ],
    [ "E3AP: semantics and constraints", "md_docs_2e3ap_205-semantics-and-constraints.html", [
      [ "Units and ranges", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md354", null ],
      [ "Clocks", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md355", null ],
      [ "Absent versus zero", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md356", null ],
      [ "What is mandatory", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md357", null ],
      [ "Validation", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md358", null ],
      [ "Handling of unknown, unforeseen and erroneous data", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md359", [
        [ "Abstract syntax errors", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md360", null ],
        [ "Unexpected messages", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md361", null ],
        [ "Unknown extension additions", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md362", null ],
        [ "Unknown response codes", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md363", null ],
        [ "Unknown RAN function or stopped service model", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md364", null ],
        [ "Queues that are full", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md365", null ],
        [ "Lost links", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md366", null ]
      ] ],
      [ "Identifiers over time", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md367", null ],
      [ "Subscription time", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md368", null ]
    ] ],
    [ "E3AP: implementation notes and known discrepancies", "md_docs_2e3ap_206-discrepancies.html", [
      [ "Setup", "md_docs_2e3ap_206-discrepancies.html#autotoc_md370", null ],
      [ "Subscription and control", "md_docs_2e3ap_206-discrepancies.html#autotoc_md371", null ],
      [ "Release and link loss", "md_docs_2e3ap_206-discrepancies.html#autotoc_md372", null ],
      [ "Encoding and framing", "md_docs_2e3ap_206-discrepancies.html#autotoc_md373", null ],
      [ "Where to look", "md_docs_2e3ap_206-discrepancies.html#autotoc_md374", null ]
    ] ],
    [ "E3AP: integrator guide", "md_docs_2e3ap_2integrator-guide.html", [
      [ "Choose a route", "md_docs_2e3ap_2integrator-guide.html#autotoc_md376", null ],
      [ "Configuration of the library", "md_docs_2e3ap_2integrator-guide.html#autotoc_md377", null ],
      [ "Where each claim comes from", "md_docs_2e3ap_2integrator-guide.html#autotoc_md378", [
        [ "Identity and links (chapter 01)", "md_docs_2e3ap_2integrator-guide.html#autotoc_md379", null ],
        [ "Procedures (chapter 03)", "md_docs_2e3ap_2integrator-guide.html#autotoc_md380", null ],
        [ "Encoding and framing (chapter 04)", "md_docs_2e3ap_2integrator-guide.html#autotoc_md381", null ],
        [ "The pinned vectors", "md_docs_2e3ap_2integrator-guide.html#autotoc_md382", null ]
      ] ],
      [ "Stale comments in the code", "md_docs_2e3ap_2integrator-guide.html#autotoc_md383", null ],
      [ "Checklist for a new E3 Agent", "md_docs_2e3ap_2integrator-guide.html#autotoc_md384", null ]
    ] ],
    [ "E3SM-L1KPM: identity and registration", "md_docs_2e3sm_2l1-kpm_201-identity-and-registration.html", [
      [ "Identity", "md_docs_2e3sm_2l1-kpm_201-identity-and-registration.html#autotoc_md400", null ],
      [ "Telemetry identifiers", "md_docs_2e3sm_2l1-kpm_201-identity-and-registration.html#autotoc_md401", null ],
      [ "RAN function data", "md_docs_2e3sm_2l1-kpm_201-identity-and-registration.html#autotoc_md402", null ],
      [ "What a RAN shall do to offer the service model", "md_docs_2e3sm_2l1-kpm_201-identity-and-registration.html#autotoc_md403", null ],
      [ "What a dApp shall do to use the service model", "md_docs_2e3sm_2l1-kpm_201-identity-and-registration.html#autotoc_md404", null ]
    ] ],
    [ "E3SM-L1KPM: information elements", "md_docs_2e3sm_2l1-kpm_202-ie-mapping.html", [
      [ "The elements at a glance", "md_docs_2e3sm_2l1-kpm_202-ie-mapping.html#autotoc_md406", null ],
      [ "L1KPM-Indication", "md_docs_2e3sm_2l1-kpm_202-ie-mapping.html#autotoc_md407", null ],
      [ "L1KPM-ShmRef", "md_docs_2e3sm_2l1-kpm_202-ie-mapping.html#autotoc_md408", null ],
      [ "L1KPM-RanFunctionData", "md_docs_2e3sm_2l1-kpm_202-ie-mapping.html#autotoc_md409", null ],
      [ "The shared memory region", "md_docs_2e3sm_2l1-kpm_202-ie-mapping.html#autotoc_md410", [
        [ "Region layout", "md_docs_2e3sm_2l1-kpm_202-ie-mapping.html#autotoc_md411", null ],
        [ "Header", "md_docs_2e3sm_2l1-kpm_202-ie-mapping.html#autotoc_md412", null ],
        [ "IQ row", "md_docs_2e3sm_2l1-kpm_202-ie-mapping.html#autotoc_md413", null ],
        [ "Sample format", "md_docs_2e3sm_2l1-kpm_202-ie-mapping.html#autotoc_md414", null ]
      ] ]
    ] ],
    [ "E3SM-L1KPM: procedure flow", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html", [
      [ "Overview", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md416", null ],
      [ "Setup", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md417", [
        [ "Purpose", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md418", null ],
        [ "Procedure text", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md419", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md420", null ]
      ] ],
      [ "Subscription", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md421", [
        [ "Purpose", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md422", null ],
        [ "Procedure text", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md423", null ],
        [ "Successful operation", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md424", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md425", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md426", null ]
      ] ],
      [ "Indication", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md427", [
        [ "Purpose", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md428", null ],
        [ "Procedure text on the RAN side", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md429", null ],
        [ "Procedure text on the dApp side", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md430", null ],
        [ "Successful operation", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md431", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md432", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md433", null ]
      ] ],
      [ "Overwrite detection", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md434", null ],
      [ "Release", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md435", [
        [ "Purpose", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md436", null ],
        [ "Procedure text", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md437", null ],
        [ "Successful operation", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md438", null ]
      ] ],
      [ "Unsupported control", "md_docs_2e3sm_2l1-kpm_203-procedure-flow.html#autotoc_md439", null ]
    ] ],
    [ "E3SM-L1KPM: encoding", "md_docs_2e3sm_2l1-kpm_204-encoding.html", [
      [ "Choice of encoding", "md_docs_2e3sm_2l1-kpm_204-encoding.html#autotoc_md441", null ],
      [ "ASN.1 APER", "md_docs_2e3sm_2l1-kpm_204-encoding.html#autotoc_md442", null ],
      [ "JSON", "md_docs_2e3sm_2l1-kpm_204-encoding.html#autotoc_md443", null ],
      [ "Protocol Buffers", "md_docs_2e3sm_2l1-kpm_204-encoding.html#autotoc_md444", null ],
      [ "Size limits", "md_docs_2e3sm_2l1-kpm_204-encoding.html#autotoc_md445", null ],
      [ "Abstract syntax", "md_docs_2e3sm_2l1-kpm_204-encoding.html#autotoc_md446", null ]
    ] ],
    [ "E3SM-L1KPM: semantics and constraints", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html", [
      [ "Units and clocks", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md448", null ],
      [ "Optional and absent values", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md449", null ],
      [ "The shared memory region", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md450", [
        [ "Lifetime", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md451", null ],
        [ "Same host rule", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md452", null ],
        [ "Rows and overwrite", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md453", null ],
        [ "Antennas and subcarriers", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md454", null ],
        [ "Scale factor", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md455", null ]
      ] ],
      [ "Mandatory behavior", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md456", null ],
      [ "Validation", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md457", null ],
      [ "Error handling", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md458", null ],
      [ "Extensibility", "md_docs_2e3sm_2l1-kpm_205-semantics-and-constraints.html#autotoc_md459", null ]
    ] ],
    [ "E3SM-L1KPM: discrepancies, TODOs and dead code", "md_docs_2e3sm_2l1-kpm_206-discrepancies.html", [
      [ "Where the grammar and the behavior disagree", "md_docs_2e3sm_2l1-kpm_206-discrepancies.html#autotoc_md461", null ],
      [ "Where the encodings disagree", "md_docs_2e3sm_2l1-kpm_206-discrepancies.html#autotoc_md462", null ],
      [ "Shared memory", "md_docs_2e3sm_2l1-kpm_206-discrepancies.html#autotoc_md463", null ],
      [ "Dead and unused code", "md_docs_2e3sm_2l1-kpm_206-discrepancies.html#autotoc_md464", null ],
      [ "Could not be tied to code", "md_docs_2e3sm_2l1-kpm_206-discrepancies.html#autotoc_md465", null ],
      [ "Open items", "md_docs_2e3sm_2l1-kpm_206-discrepancies.html#autotoc_md466", null ]
    ] ],
    [ "E3SM-L1KPM: integrator guide", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html", [
      [ "Code roots", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md468", null ],
      [ "Where the pieces are", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md469", null ],
      [ "For a RAN", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md470", [
        [ "Steps", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md471", null ],
        [ "Things that bite", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md472", null ]
      ] ],
      [ "For a dApp", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md473", null ],
      [ "Evidence for the discrepancies", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md474", null ]
    ] ],
    [ "E3SM-SPEC: identity and registration", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html", [
      [ "Identity", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md493", null ],
      [ "Telemetry and control identifiers", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md494", null ],
      [ "The RAN function data", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md495", null ],
      [ "How a dApp finds the service model", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md496", null ],
      [ "What a RAN shall provide", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md497", null ],
      [ "Lifecycle", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md498", null ]
    ] ],
    [ "E3SM-SPEC: information elements", "md_docs_2e3sm_2spectrum_202-ie-mapping.html", [
      [ "Overview", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md500", null ],
      [ "RAN function data", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md501", null ],
      [ "Sensing indication", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md502", null ],
      [ "dApp control", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md503", [
        [ "Envelope", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md504", null ],
        [ "PRB block control", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md505", null ],
        [ "Sensing policy control", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md506", null ]
      ] ],
      [ "dApp report", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md507", null ],
      [ "xApp control", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md508", null ],
      [ "Apply outcome", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md509", null ],
      [ "Constants", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md510", null ],
      [ "The sensing ring", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md511", null ]
    ] ],
    [ "E3SM-SPEC: procedures", "md_docs_2e3sm_2spectrum_203-procedure-flow.html", [
      [ "Overview", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md513", null ],
      [ "Subscription", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md514", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md515", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md516", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md517", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md518", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md519", null ]
      ] ],
      [ "Sensing indication", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md520", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md521", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md522", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md523", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md524", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md525", null ]
      ] ],
      [ "PRB block control", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md526", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md527", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md528", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md529", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md530", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md531", null ]
      ] ],
      [ "Sensing policy control", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md532", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md533", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md534", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md535", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md536", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md537", null ]
      ] ],
      [ "dApp report", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md538", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md539", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md540", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md541", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md542", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md543", null ]
      ] ],
      [ "xApp control relay", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md544", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md545", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md546", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md547", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md548", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md549", null ]
      ] ],
      [ "Apply outcome", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md550", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md551", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md552", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md553", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md554", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md555", null ]
      ] ],
      [ "Service model stop", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md556", null ],
      [ "The loop from end to end", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md557", null ]
    ] ],
    [ "E3SM-SPEC: encoding", "md_docs_2e3sm_2spectrum_204-encoding.html", [
      [ "Principles", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md559", null ],
      [ "APER", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md560", [
        [ "Extension marker and extension alternative", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md561", null ]
      ] ],
      [ "JSON", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md562", [
        [ "RAN function data", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md563", null ],
        [ "Sensing indication", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md564", null ],
        [ "dApp control", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md565", null ],
        [ "Unknown keys and ranges", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md566", null ],
        [ "dApp report and xApp control", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md567", null ]
      ] ],
      [ "Protocol Buffers", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md568", null ],
      [ "Carrying in E3AP", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md569", null ],
      [ "Size limits", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md570", null ],
      [ "Abstract syntax with ASN.1", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md571", null ]
    ] ],
    [ "E3SM-SPEC: semantics and constraints", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html", [
      [ "Units", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md573", null ],
      [ "Clocks", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md574", null ],
      [ "Absent and zero", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md575", null ],
      [ "PRB block semantics", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md576", null ],
      [ "Limits of a PRB block", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md577", null ],
      [ "Sensing policy semantics", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md578", null ],
      [ "What does and does not touch the radio", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md579", null ],
      [ "Validation", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md580", null ],
      [ "Unknown identifiers and alternatives", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md581", null ],
      [ "Concurrency and freshness of the ring", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md582", null ],
      [ "What is mandatory", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md583", null ]
    ] ],
    [ "E3SM-SPEC: discrepancies, dead fields and open items", "md_docs_2e3sm_2spectrum_206-discrepancies.html", [
      [ "Two copies of the grammar", "md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md585", null ],
      [ "The sensing indication has three spellings", "md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md586", null ],
      [ "Fields that the RAN decodes and does not use", "md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md587", null ],
      [ "The three decoders have different strictness", "md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md588", null ],
      [ "The sensing policy needs a mask to be cleared", "md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md589", null ],
      [ "Fields that are wider than their values", "md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md590", null ],
      [ "Fields and alternatives without a codec", "md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md591", null ],
      [ "The xApp leg has no JSON codec", "md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md592", null ],
      [ "Dead code", "md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md593", null ],
      [ "Revisions that do not match", "md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md594", null ],
      [ "Open items", "md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md595", null ]
    ] ],
    [ "E3SM-SPEC: integrator guide", "md_docs_2e3sm_2spectrum_2integrator-guide.html", [
      [ "Where the code references point", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md597", null ],
      [ "Artifacts", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md598", null ],
      [ "The RAN side", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md599", [
        [ "Register the service model", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md600", null ],
        [ "Telemetry", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md601", null ],
        [ "Controls", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md602", null ],
        [ "The report and the xApp control", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md603", null ],
        [ "The apply outcome", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md604", null ]
      ] ],
      [ "The dApp side", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md605", null ],
      [ "The xApp side", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md606", null ],
      [ "Evidence for the discrepancies", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md607", null ],
      [ "Tests", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md608", null ]
    ] ],
    [ "Interoperability", "md_docs_2interop.html", [
      [ "What pairs with what", "md_docs_2interop.html#autotoc_md624", null ],
      [ "JSON as libe3 speaks it", "md_docs_2interop.html#autotoc_md625", null ],
      [ "Field contracts", "md_docs_2interop.html#autotoc_md626", null ],
      [ "Conformance fixtures", "md_docs_2interop.html#autotoc_md627", null ],
      [ "When the RAN goes away", "md_docs_2interop.html#autotoc_md628", null ],
      [ "Known differences", "md_docs_2interop.html#autotoc_md629", null ],
      [ "Found while writing the specifications", "md_docs_2interop.html#autotoc_md630", null ],
      [ "Aerial: what to change to align", "md_docs_2interop.html#autotoc_md631", null ],
      [ "OCUDU: what to change to align", "md_docs_2interop.html#autotoc_md632", null ],
      [ "Change ledger", "md_docs_2interop.html#autotoc_md633", null ]
    ] ],
    [ "Latency recording (latrec)", "md_docs_2utilities_2latrec.html", [
      [ "Why a ring, and why one gate", "md_docs_2utilities_2latrec.html#autotoc_md635", [
        [ "Matching the flag downstream", "md_docs_2utilities_2latrec.html#autotoc_md636", null ],
        [ "Python (libe3py)", "md_docs_2utilities_2latrec.html#autotoc_md637", null ],
        [ "Where the rings go", "md_docs_2utilities_2latrec.html#autotoc_md638", null ]
      ] ],
      [ "Clock model", "md_docs_2utilities_2latrec.html#autotoc_md639", null ],
      [ "Ring naming and sizing", "md_docs_2utilities_2latrec.html#autotoc_md640", null ],
      [ "Stage catalog", "md_docs_2utilities_2latrec.html#autotoc_md641", [
        [ "Joining across components", "md_docs_2utilities_2latrec.html#autotoc_md642", null ],
        [ "Attribution, and where the ring name stops being enough", "md_docs_2utilities_2latrec.html#autotoc_md643", [
          [ "Possible follow-up: tier-aware attribution", "md_docs_2utilities_2latrec.html#autotoc_md644", null ]
        ] ],
        [ "Granularity, and adding an identifier", "md_docs_2utilities_2latrec.html#autotoc_md645", null ]
      ] ],
      [ "Capture → CSV workflow", "md_docs_2utilities_2latrec.html#autotoc_md646", null ],
      [ "CI overhead gate", "md_docs_2utilities_2latrec.html#autotoc_md647", null ]
    ] ],
    [ "Contributing", "md_CONTRIBUTING.html", [
      [ "Pull Request Process", "md_CONTRIBUTING.html#autotoc_md679", null ],
      [ "Development Workflow", "md_CONTRIBUTING.html#autotoc_md680", [
        [ "For maintainers and existing collaborators", "md_CONTRIBUTING.html#autotoc_md681", null ],
        [ "For external contributors", "md_CONTRIBUTING.html#autotoc_md682", null ],
        [ "Merging (maintainers)", "md_CONTRIBUTING.html#autotoc_md683", null ]
      ] ],
      [ "Mandatory checks", "md_CONTRIBUTING.html#autotoc_md684", [
        [ "Issues", "md_CONTRIBUTING.html#autotoc_md685", null ],
        [ "Pull requests", "md_CONTRIBUTING.html#autotoc_md686", null ],
        [ "Licensing headers", "md_CONTRIBUTING.html#autotoc_md687", null ],
        [ "AI assistants", "md_CONTRIBUTING.html#autotoc_md688", null ],
        [ "Service Models and twin-repo coordination", "md_CONTRIBUTING.html#autotoc_md689", null ],
        [ "Specifications and their Word editions", "md_CONTRIBUTING.html#autotoc_md690", null ]
      ] ],
      [ "Code of Conduct", "md_CONTRIBUTING.html#autotoc_md691", [
        [ "Our Pledge", "md_CONTRIBUTING.html#autotoc_md692", null ],
        [ "Our Standards", "md_CONTRIBUTING.html#autotoc_md693", null ],
        [ "Our Responsibilities", "md_CONTRIBUTING.html#autotoc_md694", null ],
        [ "Scope", "md_CONTRIBUTING.html#autotoc_md695", null ],
        [ "Enforcement", "md_CONTRIBUTING.html#autotoc_md696", null ],
        [ "Attribution", "md_CONTRIBUTING.html#autotoc_md697", null ]
      ] ]
    ] ],
    [ "Topics", "topics.html", "topics" ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ],
        [ "Enumerator", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"classlibe3_1_1E3Agent.html#af5f062ab236a0693446a98fad7512dd9",
"classlibe3_1_1Logger.html",
"classlibe3_1_1ZmqE3Connector.html#aea801c0333dfa5368343917c23e8fb0d",
"functions_f.html",
"md_docs_2e2sm-dapp_202-ie-mapping.html",
"md_docs_2e3ap_203-procedure-flow.html#autotoc_md304",
"md_docs_2e3sm_2spectrum_206-discrepancies.html#autotoc_md585",
"structe3__service__model__handle__s.html#ad9725ffd222d071abb7a158aade904da",
"structlibe3_1_1SubscriptionDelete.html#ad1cd6bc2b44243778e336bd954c2af72",
"structlibe3_1_1e2sm__dapp_1_1e2sm__dapp__ctrl__outcome__s.html",
"structlibe3_1_1e2sm__dapp_1_1e2sm__dapp__ind__msg__s_1_1ric__ind__msg__formats__c___1_1types__opts.html#aa8fcc226fee30cf9c917df96329a2a9a",
"structlibe3__e2sm__dapp__report__style__t.html#ac54625f62b533d0b65265e08e7e0c73c"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
var LISTOFALLMEMBERS = 'List of all members';