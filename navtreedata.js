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
      [ "Box order", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md10", null ],
      [ "Forward leg: RAN to dApp (<span class=\"tt\">leg=ind_up</span>)", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md11", [
        [ "Caveat on A6 and the wire", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md12", null ]
      ] ],
      [ "Return leg: dApp to RAN (<span class=\"tt\">leg=ctrl_down</span> for control, <span class=\"tt\">report_up</span> for reports)", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md13", [
        [ "Two distinct terminations", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md14", null ]
      ] ],
      [ "Aggregates", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md15", null ],
      [ "Deployment variants", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md16", [
        [ "Comparing against a hand-rolled integration", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md17", null ]
      ] ],
      [ "Not yet instrumented", "md_docs_2control-loops_2path-a-e3-loop.html#autotoc_md18", null ]
    ] ],
    [ "Path B: the full E2-E3 loop", "md_docs_2control-loops_2path-b-e2-e3-loop.html", [
      [ "Box order", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md20", null ],
      [ "The two RIC profiles", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md21", null ],
      [ "Report-up leg (<span class=\"tt\">leg=report_up</span>)", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md22", null ],
      [ "Policy-down leg (<span class=\"tt\">leg=policy_down</span>)", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md23", null ],
      [ "Mitigation leg (<span class=\"tt\">leg=mitigation</span>)", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md24", null ],
      [ "Aggregates", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md25", null ],
      [ "Joining: the <span class=\"tt\">sequenceId</span>", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md26", [
        [ "Where it shows up in the rings", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md27", null ]
      ] ],
      [ "Not yet instrumented", "md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md28", null ]
    ] ],
    [ "E2SM-DAPP: identity and registration", "md_docs_2e2sm-dapp_201-identity-and-registration.html", [
      [ "1. Identity", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md30", [
        [ "1.1 How the OID is built", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md31", null ],
        [ "1.2 Two things named \"RAN function ID\"", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md32", null ]
      ] ],
      [ "2. RAN function definition: what it contains", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md33", [
        [ "2.1 What a RAN sends", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md34", null ],
        [ "2.2 Who sets the name block", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md35", null ]
      ] ],
      [ "3. Announcement in E2 Setup and RIC Service Update", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md36", [
        [ "3.1 E2 Setup Request", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md37", null ],
        [ "3.2 RIC Service Update", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md38", null ]
      ] ],
      [ "4. Requirements for a RAN that announces E2SM-DAPP", "md_docs_2e2sm-dapp_201-identity-and-registration.html#autotoc_md39", null ]
    ] ],
    [ "E2SM-DAPP: E2AP information element mapping", "md_docs_2e2sm-dapp_202-ie-mapping.html", [
      [ "1. The elements at a glance", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md41", null ],
      [ "2. Which E2 service uses which format", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md42", [
        [ "2.1 Why INSERT, POLICY and QUERY are \"not used\"", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md43", null ],
        [ "2.2 Why there is no call process ID", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md44", null ]
      ] ],
      [ "3. Common types", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md45", null ],
      [ "4. Event trigger and action definition", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md46", [
        [ "4.1 <span class=\"tt\">E2SM-DAPP-EventTrigger</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md47", null ],
        [ "4.2 <span class=\"tt\">E2SM-DAPP-ActionDefinition</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md48", null ]
      ] ],
      [ "5. RAN function definition", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md49", [
        [ "5.1 <span class=\"tt\">RANfunction-Name</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md50", null ],
        [ "5.2 <span class=\"tt\">RANFunctionDefinition-Report</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md51", null ],
        [ "5.3 <span class=\"tt\">RANFunctionDefinition-Control</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md52", null ],
        [ "5.4 dApp E3 subscription list", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md53", null ]
      ] ],
      [ "6. Indication header and message", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md54", [
        [ "6.1 <span class=\"tt\">E2SM-DAPP-IndicationHeader</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md55", null ],
        [ "6.2 <span class=\"tt\">E2SM-DAPP-IndicationMessage</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md56", null ]
      ] ],
      [ "7. Control header, message, outcome", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md57", [
        [ "7.1 <span class=\"tt\">E2SM-DAPP-ControlHeader</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md58", null ],
        [ "7.2 <span class=\"tt\">E2SM-DAPP-ControlMessage</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md59", null ],
        [ "7.3 <span class=\"tt\">E2SM-DAPP-ControlOutcome</span>", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md60", null ]
      ] ],
      [ "8. The <span class=\"tt\">e3-control-outcome</span> payload", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md61", [
        [ "8.1 Who owns what", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md62", null ],
        [ "8.2 Worked example: the Spectrum service model", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md63", null ],
        [ "8.3 How a consumer reads it", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md64", null ],
        [ "8.4 How another service model adds its own outcome", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md65", null ]
      ] ],
      [ "9. Where the elements are produced and consumed", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md66", null ],
      [ "10. Other representations of the elements", "md_docs_2e2sm-dapp_202-ie-mapping.html#autotoc_md67", null ]
    ] ],
    [ "E2SM-DAPP: procedure flow", "md_docs_2e2sm-dapp_203-procedure-flow.html", [
      [ "Roles", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md69", null ],
      [ "Messages on the flow", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md70", null ],
      [ "Discovery: how an xApp learns the dApp ids", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md71", [
        [ "In the RAN function definition", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md72", null ],
        [ "When the set of dApps changes", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md73", null ],
        [ "In every report header", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md74", null ],
        [ "What discovery does not do", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md75", null ]
      ] ],
      [ "Subscription: pairing the xApp with the report stream", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md76", null ],
      [ "Indication delivery (report up)", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md77", null ],
      [ "Control relay and the deferred acknowledge", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md78", [
        [ "Policy down: from the xApp to the dApp", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md79", null ],
        [ "Mitigation: from the dApp back to the air", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md80", null ],
        [ "The E2 acknowledge is deferred to B19", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md81", null ],
        [ "The mitigation figure", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md82", null ]
      ] ],
      [ "The loop on the boxes", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md83", [
        [ "The sequence-id chain", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md84", null ],
        [ "The same flow as a sequence", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md85", null ]
      ] ],
      [ "Differences between implementations", "md_docs_2e2sm-dapp_203-procedure-flow.html#autotoc_md86", null ]
    ] ],
    [ "E2SM-DAPP: encoding", "md_docs_2e2sm-dapp_204-encoding.html", [
      [ "What is on the wire", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md88", [
        [ "The three rules that decide almost every byte", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md89", null ],
        [ "Encoding rules, by ASN.1 construct", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md90", null ],
        [ "Byte order, packing, length prefixes", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md91", null ],
        [ "A receiver that sees extension additions", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md92", null ]
      ] ],
      [ "Maximum sizes", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md93", null ],
      [ "Worked examples", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md94", [
        [ "<span class=\"tt\">et_f1</span> = <span class=\"tt\">00</span> (event trigger)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md95", null ],
        [ "<span class=\"tt\">ad_style1</span> = <span class=\"tt\">00 01 01 00</span> (action definition, report style 1)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md96", null ],
        [ "<span class=\"tt\">ih1_min</span> = <span class=\"tt\">00 00 ff 00 01 00 00 13 00 14 00 00</span> (indication header format 1, only mandatory fields)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md97", null ],
        [ "<span class=\"tt\">ih1_full</span> = <span class=\"tt\">0e 00 ff 00 07 00 02 00 f1 10 20 04 d2 00 05 08 17 97 9c fe 3d 85 cd 15 01 2a</span>", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md98", null ],
        [ "<span class=\"tt\">ch1_min</span> = <span class=\"tt\">00 01 00 02</span> (control header, no timestamp, no sequence-id)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md99", null ],
        [ "<span class=\"tt\">co1_full</span> = <span class=\"tt\">1c 08 17979cfe3d85cd15 01 2a 0a 10 20 30 40 50 60 70 80 90 a0</span> (control outcome, everything present)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md100", null ],
        [ "Two APER layers nest in the control outcome", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md101", null ],
        [ "<span class=\"tt\">im1_small</span> = <span class=\"tt\">00 00 04 00 03 de ad be ef</span> (indication message format 1, 4 octets of payload)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md102", null ],
        [ "<span class=\"tt\">im2_one</span> = <span class=\"tt\">20 00 01 00 01 00 00 01</span> (indication message format 2, one dApp, one function)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md103", null ],
        [ "<span class=\"tt\">fd_min</span>, the smallest RAN function definition", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md104", null ]
      ] ],
      [ "Reference test vectors (full list)", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md105", null ],
      [ "Native types for the grammar types", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md106", null ],
      [ "Checking an encoding", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md107", null ],
      [ "Abstract syntax", "md_docs_2e2sm-dapp_204-encoding.html#autotoc_md108", null ]
    ] ],
    [ "E2SM-DAPP: semantics and constraints", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html", [
      [ "1. Numbering: formats and styles", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md110", null ],
      [ "2. Identifiers", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md111", null ],
      [ "3. Clocks and units", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md112", [
        [ "Whose clock is each timestamp on", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md113", null ]
      ] ],
      [ "4. Optional fields: absent is not an error", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md114", null ],
      [ "5. What is mandatory", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md115", null ],
      [ "6. Validation and error handling", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md116", [
        [ "6.1 On encode", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md117", null ],
        [ "6.2 On decode", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md118", null ],
        [ "6.3 Error paths in summary", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md119", null ]
      ] ],
      [ "7. Behavior on unknown ids and formats", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md120", null ],
      [ "8. Control semantics", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md121", [
        [ "8.1 What \"applied\" means, and what the acknowledge says", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md122", null ],
        [ "8.2 What a consumer shall do", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md123", null ],
        [ "8.3 Ordering and replacement", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md124", null ]
      ] ],
      [ "9. Rules that cross elements", "md_docs_2e2sm-dapp_205-semantics-and-constraints.html#autotoc_md125", null ]
    ] ],
    [ "E2SM-DAPP: code pointers and evidence", "md_docs_2e2sm-dapp_2code-pointers.html", [
      [ "Chapter README: Artifacts and code roots", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md127", [
        [ "Where each part is specified", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md128", null ],
        [ "Where the artifacts are", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md129", null ],
        [ "Related documents", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md130", null ],
        [ "Where the code references point", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md131", null ]
      ] ],
      [ "Chapter 1: Identity and registration (code evidence)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md132", [
        [ "Identity values and where they are defined", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md133", null ],
        [ "The OID comment", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md134", null ],
        [ "Two kinds of \"RAN function ID\"", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md135", null ],
        [ "Where the identity is written down (section 1.3 of the old text)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md136", null ],
        [ "What a real RAN sends (old section 2.1)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md137", null ],
        [ "What flexric's agent adds or replaces (old section 2.2)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md138", null ],
        [ "E2 Setup Request (old section 3.1)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md139", null ],
        [ "RIC Service Update (old section 3.2)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md140", null ],
        [ "Checklist notes (old section 4)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md141", null ]
      ] ],
      [ "Chapter 2: E2AP information element mapping (code evidence)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md142", [
        [ "Style numbers, formats and names (old section 2, \"Where each fact comes from\")", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md143", null ],
        [ "Why INSERT, POLICY and QUERY are \"not used\"", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md144", null ],
        [ "Why there is no call process ID", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md145", null ],
        [ "Common types", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md146", null ],
        [ "Event trigger and action definition", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md147", null ],
        [ "RAN function definition", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md148", null ],
        [ "Indication header", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md149", null ],
        [ "Control header and outcome", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md150", null ],
        [ "The Spectrum outcome worked example (old section 8.2)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md151", null ],
        [ "How a consumer reads the outcome (old section 8.3)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md152", null ],
        [ "E3 RAN function id registry (old section 8.4)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md153", null ],
        [ "Where the elements are produced and consumed (old section 9)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md154", null ],
        [ "The same elements in libe3 and OCUDU (old section 10)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md155", null ]
      ] ],
      [ "Chapter 3: Procedure flow (code evidence moved out of 03-procedure-flow.md)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md156", [
        [ "The cast: who is who in the code", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md157", null ],
        [ "Every message on the flow", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md158", null ],
        [ "Discovery: the RAN function definition (E2 Setup and RIC Service Update)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md159", null ],
        [ "When the set of dApps changes", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md160", null ],
        [ "Discovery in every report header", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md161", null ],
        [ "What discovery does not do", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md162", null ],
        [ "Subscription", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md163", null ],
        [ "Indication delivery (report up)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md164", null ],
        [ "Control relay: policy down", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md165", null ],
        [ "Control relay: mitigation back to the air", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md166", null ],
        [ "The deferred acknowledge", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md167", null ],
        [ "The mitigation figure", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md168", null ],
        [ "The sequence-id chain", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md169", null ],
        [ "The same flow in each implementation", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md170", null ]
      ] ],
      [ "Chapter 4: Encoding (code evidence moved out of 04-encoding.md)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md171", [
        [ "What is on the wire", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md172", null ],
        [ "Byte order, packing, length prefixes", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md173", null ],
        [ "A receiver that sees extension additions", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md174", null ],
        [ "Implementation limits tighter than the grammar", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md175", null ],
        [ "Worked examples: where the bytes come from", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md176", null ],
        [ "Golden vectors (full list)", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md177", null ],
        [ "The type map across implementations", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md178", [
          [ "Type names", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md179", null ],
          [ "How the same field is held", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md180", null ],
          [ "The struct layout of flexric is an ABI", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md181", null ]
        ] ],
        [ "Build switches", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md182", [
          [ "flexric: <span class=\"tt\">SM_ENCODING_DAPP</span>", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md183", null ],
          [ "libe3: <span class=\"tt\">LIBE3_ENABLE_E2SM_DAPP</span>", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md184", null ],
          [ "OCUDU: <span class=\"tt\">e2sm_dapp_enabled</span>", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md185", null ]
        ] ],
        [ "Reproducing and checking an encoding", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md186", null ]
      ] ],
      [ "Chapter 5: Semantics and constraints", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md187", [
        [ "1. Numbering: formats and styles", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md188", null ],
        [ "2. Identifiers", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md189", null ],
        [ "3. Clocks and units", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md190", null ],
        [ "Whose clock is each timestamp on", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md191", null ],
        [ "4. Optional fields: absent is not an error", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md192", null ],
        [ "5. What the code treats as mandatory", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md193", null ],
        [ "6. What each implementation checks", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md194", [
          [ "6.1 On encode", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md195", null ],
          [ "6.2 On decode", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md196", null ],
          [ "6.3 Error paths in summary", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md197", null ]
        ] ],
        [ "7. Behavior on unknown ids and formats", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md198", null ],
        [ "8. Control semantics", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md199", [
          [ "8.1 What \"applied\" means, and what the acknowledge says", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md200", null ],
          [ "8.2 What a consumer must do", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md201", null ],
          [ "8.3 Ordering and replacement", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md202", null ]
        ] ],
        [ "9. Rules that cross elements", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md203", null ]
      ] ],
      [ "Chapter 6: Discrepancies, TODOs and dead code", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md204", [
        [ "1. Dead code", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md205", null ],
        [ "2. Where the grammar and the code disagree", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md206", null ],
        [ "3. Aborts and error handling in flexric", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md207", null ],
        [ "4. Consumers", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md208", null ],
        [ "5. Differences between the implementations", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md209", [
          [ "5.1 OCUDU's E2 module against flexric and OAI", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md210", null ],
          [ "5.2 OCUDU runtime helpers the port works around", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md211", null ]
        ] ],
        [ "6. What the golden vectors prove and what they do not", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md212", null ],
        [ "7. Open items", "md_docs_2e2sm-dapp_2code-pointers.html#autotoc_md213", null ]
      ] ]
    ] ],
    [ "E2SM-DAPP: integrator guide", "md_docs_2e2sm-dapp_2integrator-guide.html", [
      [ "Choose a route", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md215", null ],
      [ "Route A: link libe3", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md216", [
        [ "Build and install libe3", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md217", null ],
        [ "Use it from CMake", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md218", null ],
        [ "C++: pack an indication header and indication message", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md219", null ],
        [ "C++: unpack a control and read it", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md220", null ],
        [ "C++: build the control outcome (the RAN side of an acknowledge)", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md221", null ],
        [ "C: the same, from C", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md222", null ]
      ] ],
      [ "Route B: run asn1c yourself", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md223", [
        [ "The asn1c you must use", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md224", null ],
        [ "The exact flags", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md225", null ],
        [ "Compile definitions and layout", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md226", null ]
      ] ],
      [ "Route C: a hand-written codec (OCUDU)", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md227", null ],
      [ "The OCUDU to libe3 C++ interface", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md228", null ],
      [ "The contract: the golden vectors", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md229", [
        [ "The shared test-fixture technique", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md230", null ],
        [ "Reproducing the golden vectors", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md231", null ]
      ] ],
      [ "Checklist for a new stack", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md232", null ],
      [ "What is deliberately not here", "md_docs_2e2sm-dapp_2integrator-guide.html#autotoc_md233", null ]
    ] ],
    [ "E3AP: identity and registration", "md_docs_2e3ap_201-identity-and-registration.html", [
      [ "Identity", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md250", [
        [ "Two sides of one interface", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md251", null ],
        [ "Identifiers", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md252", null ]
      ] ],
      [ "Links and transports", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md253", [
        [ "The three channels", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md254", null ],
        [ "Default endpoints", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md255", null ]
      ] ],
      [ "How a dApp registers", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md256", [
        [ "What the RAN function list holds", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md257", null ]
      ] ],
      [ "Requirements for an E3 Agent", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md258", null ],
      [ "Related", "md_docs_2e3ap_201-identity-and-registration.html#autotoc_md259", null ]
    ] ],
    [ "E3AP: information elements", "md_docs_2e3ap_202-ie-mapping.html", [
      [ "The messages at a glance", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md261", null ],
      [ "Common types", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md262", null ],
      [ "E3-PDU", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md263", null ],
      [ "E3-SetupRequest", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md264", null ],
      [ "E3-SetupResponse", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md265", null ],
      [ "E3-RanFunctionDefinition", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md266", null ],
      [ "E3-SubscriptionRequest", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md267", null ],
      [ "E3-SubscriptionResponse", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md268", null ],
      [ "E3-SubscriptionDelete", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md269", null ],
      [ "E3-IndicationMessage", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md270", null ],
      [ "E3-DAppControlAction", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md271", null ],
      [ "E3-DAppReport", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md272", null ],
      [ "E3-XAppControlAction", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md273", null ],
      [ "E3-ReleaseMessage", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md274", null ],
      [ "E3-MessageAck", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md275", null ],
      [ "Presence notation", "md_docs_2e3ap_202-ie-mapping.html#autotoc_md276", null ]
    ] ],
    [ "E3AP: procedure flow", "md_docs_2e3ap_203-procedure-flow.html", [
      [ "Roles", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md278", null ],
      [ "Messages on the flow", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md279", null ],
      [ "Setup", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md280", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md281", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md282", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md283", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md284", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md285", null ]
      ] ],
      [ "Subscription", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md286", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md287", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md288", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md289", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md290", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md291", null ]
      ] ],
      [ "Subscription delete", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md292", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md293", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md294", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md295", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md296", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md297", null ]
      ] ],
      [ "Indication", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md298", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md299", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md300", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md301", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md302", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md303", null ]
      ] ],
      [ "dApp Control", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md304", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md305", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md306", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md307", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md308", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md309", null ]
      ] ],
      [ "dApp Report", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md310", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md311", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md312", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md313", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md314", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md315", null ]
      ] ],
      [ "xApp Control", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md316", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md317", null ],
        [ "Procedure", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md318", null ],
        [ "Successful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md319", null ],
        [ "Unsuccessful operation", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md320", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md321", null ]
      ] ],
      [ "Release", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md322", [
        [ "Purpose", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md323", null ],
        [ "Procedure from the dApp", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md324", null ],
        [ "Procedure from the E3 Agent", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md325", null ],
        [ "Abnormal conditions", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md326", null ]
      ] ],
      [ "The sequence identifier chain", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md327", null ],
      [ "Threads and queues", "md_docs_2e3ap_203-procedure-flow.html#autotoc_md328", null ]
    ] ],
    [ "E3AP: encoding", "md_docs_2e3ap_204-encoding.html", [
      [ "What is on the wire", "md_docs_2e3ap_204-encoding.html#autotoc_md330", null ],
      [ "APER", "md_docs_2e3ap_204-encoding.html#autotoc_md331", [
        [ "The rules that decide almost every byte", "md_docs_2e3ap_204-encoding.html#autotoc_md332", null ],
        [ "Rules for each construct", "md_docs_2e3ap_204-encoding.html#autotoc_md333", null ],
        [ "Worked examples", "md_docs_2e3ap_204-encoding.html#autotoc_md334", null ],
        [ "Size of the encoding", "md_docs_2e3ap_204-encoding.html#autotoc_md335", null ],
        [ "A receiver that sees extension additions", "md_docs_2e3ap_204-encoding.html#autotoc_md336", null ]
      ] ],
      [ "JSON", "md_docs_2e3ap_204-encoding.html#autotoc_md337", null ],
      [ "Protobuf", "md_docs_2e3ap_204-encoding.html#autotoc_md338", null ],
      [ "Framing on each link", "md_docs_2e3ap_204-encoding.html#autotoc_md339", [
        [ "ZeroMQ", "md_docs_2e3ap_204-encoding.html#autotoc_md340", null ],
        [ "POSIX sockets", "md_docs_2e3ap_204-encoding.html#autotoc_md341", null ]
      ] ],
      [ "Size limits", "md_docs_2e3ap_204-encoding.html#autotoc_md342", null ],
      [ "Abstract syntax", "md_docs_2e3ap_204-encoding.html#autotoc_md343", null ]
    ] ],
    [ "E3AP: semantics and constraints", "md_docs_2e3ap_205-semantics-and-constraints.html", [
      [ "Units and ranges", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md345", null ],
      [ "Clocks", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md346", null ],
      [ "Absent versus zero", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md347", null ],
      [ "What is mandatory", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md348", null ],
      [ "Validation", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md349", null ],
      [ "Handling of unknown, unforeseen and erroneous data", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md350", [
        [ "Abstract syntax errors", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md351", null ],
        [ "Unexpected messages", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md352", null ],
        [ "Unknown extension additions", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md353", null ],
        [ "Unknown response codes", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md354", null ],
        [ "Unknown cause", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md355", null ],
        [ "Unknown RAN function or stopped service model", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md356", null ],
        [ "Queues that are full", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md357", null ],
        [ "Lost links", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md358", null ]
      ] ],
      [ "Identifiers over time", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md359", null ],
      [ "Subscription time", "md_docs_2e3ap_205-semantics-and-constraints.html#autotoc_md360", null ]
    ] ],
    [ "E3AP: integrator guide", "md_docs_2e3ap_2integrator-guide.html", [
      [ "Choose a route", "md_docs_2e3ap_2integrator-guide.html#autotoc_md362", null ],
      [ "Configuration of the library", "md_docs_2e3ap_2integrator-guide.html#autotoc_md363", null ],
      [ "Where each claim comes from", "md_docs_2e3ap_2integrator-guide.html#autotoc_md364", [
        [ "Identity and links (chapter 01)", "md_docs_2e3ap_2integrator-guide.html#autotoc_md365", null ],
        [ "Procedures (chapter 03)", "md_docs_2e3ap_2integrator-guide.html#autotoc_md366", null ],
        [ "Encoding and framing (chapter 04)", "md_docs_2e3ap_2integrator-guide.html#autotoc_md367", null ],
        [ "The pinned vectors", "md_docs_2e3ap_2integrator-guide.html#autotoc_md368", null ]
      ] ],
      [ "Stale comments in the code", "md_docs_2e3ap_2integrator-guide.html#autotoc_md369", null ],
      [ "Checklist for a new E3 Agent", "md_docs_2e3ap_2integrator-guide.html#autotoc_md370", null ]
    ] ],
    [ "E3SM services", "md_docs_2e3sm_2l1-kpm_201-services.html", [
      [ "General", "md_docs_2e3sm_2l1-kpm_201-services.html#autotoc_md386", null ],
      [ "Telemetry service", "md_docs_2e3sm_2l1-kpm_201-services.html#autotoc_md387", null ],
      [ "Control service", "md_docs_2e3sm_2l1-kpm_201-services.html#autotoc_md388", null ],
      [ "Message flow", "md_docs_2e3sm_2l1-kpm_201-services.html#autotoc_md389", null ]
    ] ],
    [ "RAN function service model description", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html", [
      [ "RAN function overview", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md391", null ],
      [ "Supported E3 services", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md392", [
        [ "Telemetry", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md393", null ],
        [ "Control", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md394", null ]
      ] ],
      [ "Indication paths", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md395", null ],
      [ "Structure of an indication", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md396", null ],
      [ "Data delivery", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md397", null ],
      [ "Procedures", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md398", [
        [ "Setup", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md399", null ],
        [ "Subscription", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md400", null ],
        [ "Indication", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md401", null ],
        [ "Set Subscription Data", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md402", null ],
        [ "Subscription delete and release", "md_docs_2e3sm_2l1-kpm_202-service-model-description.html#autotoc_md403", null ]
      ] ]
    ] ],
    [ "RAN function description", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html", [
      [ "Description", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html#autotoc_md405", null ],
      [ "RAN function identity", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html#autotoc_md406", null ],
      [ "Telemetry identifiers", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html#autotoc_md407", [
        [ "General", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html#autotoc_md408", null ],
        [ "Shared identifiers", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html#autotoc_md409", null ],
        [ "PUSCH path identifiers", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html#autotoc_md410", null ],
        [ "SRS path identifiers", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html#autotoc_md411", null ],
        [ "Additional identifiers", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html#autotoc_md412", null ]
      ] ],
      [ "Shared memory streams", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html#autotoc_md413", null ],
      [ "Control identifiers", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html#autotoc_md414", null ],
      [ "Supported E3AP messages and service model IEs", "md_docs_2e3sm_2l1-kpm_203-ran-function-description.html#autotoc_md415", null ]
    ] ],
    [ "Elements for E3SM service model", "md_docs_2e3sm_2l1-kpm_204-elements.html", [
      [ "General", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md417", null ],
      [ "Message functional definition and content", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md418", [
        [ "Messages for E3 functional procedures", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md419", [
          [ "INDICATION DATA IE", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md420", null ],
          [ "CELL DATA IE", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md421", null ],
          [ "UE DATA IE", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md422", null ],
          [ "CONTROL DATA IE", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md423", null ],
          [ "SUBSCRIPTION DATA IE", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md424", null ]
        ] ],
        [ "Messages for E3 global procedures", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md425", [
          [ "RAN FUNCTION DATA IE", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md426", null ],
          [ "STREAM DESCRIPTOR IE", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md427", null ]
        ] ]
      ] ],
      [ "Information element definitions", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md428", [
        [ "General", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md429", null ],
        [ "Float32", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md430", null ],
        [ "Complex32", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md431", null ],
        [ "Uint32", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md432", null ],
        [ "RNTI", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md433", null ],
        [ "Physical Cell ID", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md434", null ],
        [ "Symbol Bitmap", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md435", null ],
        [ "Telemetry Identifier", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md436", null ],
        [ "Timestamp", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md437", null ],
        [ "Shared Memory Reference", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md438", null ],
        [ "UE Reference", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md439", null ],
        [ "Stream Status", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md440", null ],
        [ "Delivery", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md441", null ],
        [ "Shared Memory Information", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md442", null ],
        [ "Inline Information", "md_docs_2e3sm_2l1-kpm_204-elements.html#autotoc_md443", null ]
      ] ]
    ] ],
    [ "Information element abstract syntax (with ASN.1)", "md_docs_2e3sm_2l1-kpm_205-abstract-syntax.html", [
      [ "General", "md_docs_2e3sm_2l1-kpm_205-abstract-syntax.html#autotoc_md445", null ],
      [ "Information element definitions", "md_docs_2e3sm_2l1-kpm_205-abstract-syntax.html#autotoc_md446", null ]
    ] ],
    [ "Encoding of the service model IEs", "md_docs_2e3sm_2l1-kpm_206-encoding.html", [
      [ "Encoding", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md448", [
        [ "Choice of encoding", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md449", null ],
        [ "APER", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md450", null ]
      ] ],
      [ "JSON encoding", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md451", [
        [ "General", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md452", null ],
        [ "INDICATION DATA, CELL DATA and UE DATA", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md453", null ],
        [ "RAN FUNCTION DATA", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md454", null ],
        [ "SUBSCRIPTION DATA and CONTROL DATA", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md455", null ]
      ] ],
      [ "Size limits", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md456", [
        [ "Limits", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md457", null ],
        [ "Indications above the limits", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md458", null ],
        [ "Typical sizes", "md_docs_2e3sm_2l1-kpm_206-encoding.html#autotoc_md459", null ]
      ] ]
    ] ],
    [ "Handling of unknown, unforeseen and erroneous protocol data", "md_docs_2e3sm_2l1-kpm_207-error-handling.html", [
      [ "General", "md_docs_2e3sm_2l1-kpm_207-error-handling.html#autotoc_md461", null ],
      [ "Abstract syntax errors", "md_docs_2e3sm_2l1-kpm_207-error-handling.html#autotoc_md462", null ],
      [ "Unknown fields and values", "md_docs_2e3sm_2l1-kpm_207-error-handling.html#autotoc_md463", null ],
      [ "Fields that do not match the subscription", "md_docs_2e3sm_2l1-kpm_207-error-handling.html#autotoc_md464", null ],
      [ "Erroneous control actions", "md_docs_2e3sm_2l1-kpm_207-error-handling.html#autotoc_md465", null ],
      [ "Erroneous shared memory references", "md_docs_2e3sm_2l1-kpm_207-error-handling.html#autotoc_md466", null ],
      [ "Indications that arrive late", "md_docs_2e3sm_2l1-kpm_207-error-handling.html#autotoc_md467", null ]
    ] ],
    [ "Shared memory region", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html", [
      [ "General", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md469", null ],
      [ "Region layout", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md470", null ],
      [ "Header", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md471", null ],
      [ "Rows, buffers and reuse", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md472", null ],
      [ "Location of the data", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md473", null ],
      [ "Data formats", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md474", [
        [ "IQ samples", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md475", null ],
        [ "PUSCH PDU data", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md476", null ],
        [ "PUSCH channel estimates", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md477", null ],
        [ "SRS IQ samples", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md478", null ],
        [ "SRS channel estimates", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md479", null ],
        [ "SRS SNR of each PRG", "md_docs_2e3sm_2l1-kpm_2annex-a-shared-memory.html#autotoc_md480", null ]
      ] ]
    ] ],
    [ "Example message exchange", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html", [
      [ "General", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html#autotoc_md482", null ],
      [ "IQ samples of one cell", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html#autotoc_md483", null ],
      [ "UE metrics with a UE filter", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html#autotoc_md484", [
        [ "Step 1: SETUP REQUEST", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html#autotoc_md485", null ],
        [ "Step 2: SETUP RESPONSE", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html#autotoc_md486", null ],
        [ "Step 3: SUBSCRIPTION REQUEST", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html#autotoc_md487", null ],
        [ "Step 4: SUBSCRIPTION RESPONSE", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html#autotoc_md488", null ],
        [ "Step 5: INDICATION MESSAGE", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html#autotoc_md489", null ],
        [ "Step 6: dApp CONTROL ACTION", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html#autotoc_md490", null ],
        [ "Step 7: MESSAGE ACK", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html#autotoc_md491", null ],
        [ "Step 8: INDICATION MESSAGE", "md_docs_2e3sm_2l1-kpm_2annex-b-examples.html#autotoc_md492", null ]
      ] ]
    ] ],
    [ "Open items", "md_docs_2e3sm_2l1-kpm_2annex-c-open-items.html", [
      [ "General", "md_docs_2e3sm_2l1-kpm_2annex-c-open-items.html#autotoc_md494", null ],
      [ "Interaction with E3AP", "md_docs_2e3sm_2l1-kpm_2annex-c-open-items.html#autotoc_md495", null ],
      [ "Content of the service model", "md_docs_2e3sm_2l1-kpm_2annex-c-open-items.html#autotoc_md496", null ],
      [ "Shared memory region", "md_docs_2e3sm_2l1-kpm_2annex-c-open-items.html#autotoc_md497", null ],
      [ "Protocol Buffers form of existing E3 Agents", "md_docs_2e3sm_2l1-kpm_2annex-c-open-items.html#autotoc_md498", null ]
    ] ],
    [ "E3SM-L1-KPM: integrator guide", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html", [
      [ "Code roots", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md500", null ],
      [ "Where the pieces are", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md501", null ],
      [ "For a RAN", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md502", [
        [ "Steps", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md503", null ],
        [ "Things that bite", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md504", null ]
      ] ],
      [ "For a dApp", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md505", null ],
      [ "Evidence for the items of Annex C", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md506", null ],
      [ "Code notes", "md_docs_2e3sm_2l1-kpm_2integrator-guide.html#autotoc_md507", null ]
    ] ],
    [ "E3SM-SPEC: identity and registration", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html", [
      [ "Identity", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md526", null ],
      [ "Telemetry and control identifiers", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md527", null ],
      [ "The RAN function data", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md528", null ],
      [ "How a dApp finds the service model", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md529", null ],
      [ "What a RAN shall provide", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md530", null ],
      [ "Lifecycle", "md_docs_2e3sm_2spectrum_201-identity-and-registration.html#autotoc_md531", null ]
    ] ],
    [ "E3SM-SPEC: information elements", "md_docs_2e3sm_2spectrum_202-ie-mapping.html", [
      [ "Overview", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md533", null ],
      [ "RAN function data", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md534", null ],
      [ "Indication", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md535", [
        [ "Sensing indication", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md536", null ],
        [ "Cell configuration indication", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md537", null ]
      ] ],
      [ "dApp control", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md538", [
        [ "Envelope", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md539", null ],
        [ "PRB block control", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md540", null ],
        [ "Sensing policy control", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md541", null ]
      ] ],
      [ "dApp report", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md542", null ],
      [ "xApp control", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md543", null ],
      [ "Apply outcome", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md544", null ],
      [ "Constants", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md545", null ],
      [ "The sensing ring", "md_docs_2e3sm_2spectrum_202-ie-mapping.html#autotoc_md546", null ]
    ] ],
    [ "E3SM-SPEC: procedures", "md_docs_2e3sm_2spectrum_203-procedure-flow.html", [
      [ "Overview", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md548", null ],
      [ "Subscription", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md549", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md550", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md551", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md552", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md553", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md554", null ]
      ] ],
      [ "Sensing indication", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md555", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md556", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md557", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md558", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md559", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md560", null ]
      ] ],
      [ "Cell configuration indication", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md561", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md562", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md563", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md564", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md565", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md566", null ]
      ] ],
      [ "PRB block control", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md567", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md568", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md569", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md570", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md571", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md572", null ]
      ] ],
      [ "Sensing policy control", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md573", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md574", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md575", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md576", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md577", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md578", null ]
      ] ],
      [ "dApp report", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md579", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md580", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md581", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md582", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md583", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md584", null ]
      ] ],
      [ "xApp control relay", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md585", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md586", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md587", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md588", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md589", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md590", null ]
      ] ],
      [ "Apply outcome", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md591", [
        [ "Purpose", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md592", null ],
        [ "Procedure text", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md593", null ],
        [ "Successful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md594", null ],
        [ "Unsuccessful operation", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md595", null ],
        [ "Abnormal conditions", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md596", null ]
      ] ],
      [ "Service model stop", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md597", null ],
      [ "The loop from end to end", "md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md598", null ]
    ] ],
    [ "E3SM-SPEC: encoding", "md_docs_2e3sm_2spectrum_204-encoding.html", [
      [ "Principles", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md600", null ],
      [ "APER", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md601", [
        [ "Extension marker and extension alternative", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md602", null ]
      ] ],
      [ "JSON", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md603", [
        [ "RAN function data", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md604", null ],
        [ "Indication", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md605", null ],
        [ "Sensing indication", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md606", null ],
        [ "Cell configuration indication", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md607", null ],
        [ "dApp control", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md608", null ],
        [ "Unknown keys and ranges", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md609", null ],
        [ "dApp report and xApp control", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md610", null ]
      ] ],
      [ "Protocol Buffers", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md611", null ],
      [ "Carrying in E3AP", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md612", null ],
      [ "Size limits", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md613", null ],
      [ "Abstract syntax with ASN.1", "md_docs_2e3sm_2spectrum_204-encoding.html#autotoc_md614", null ]
    ] ],
    [ "E3SM-SPEC: semantics and constraints", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html", [
      [ "Units", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md616", null ],
      [ "Clocks", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md617", null ],
      [ "Absent and zero", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md618", null ],
      [ "PRB block semantics", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md619", null ],
      [ "Limits of a PRB block", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md620", null ],
      [ "Sensing policy semantics", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md621", null ],
      [ "What does and does not touch the radio", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md622", null ],
      [ "Validation", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md623", null ],
      [ "Unknown identifiers and alternatives", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md624", null ],
      [ "Concurrency and freshness of the ring", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md625", null ],
      [ "What is mandatory", "md_docs_2e3sm_2spectrum_205-semantics-and-constraints.html#autotoc_md626", null ]
    ] ],
    [ "E3SM-SPEC: example message exchange", "md_docs_2e3sm_2spectrum_207-examples.html", [
      [ "General", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md628", null ],
      [ "PRB block loop with an xApp", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md629", [
        [ "Step 1: E3 SETUP REQUEST", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md630", null ],
        [ "Step 2: E3 SETUP RESPONSE", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md631", null ],
        [ "Step 3: E3 SUBSCRIPTION REQUEST", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md632", null ],
        [ "Step 4: E3 SUBSCRIPTION RESPONSE", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md633", null ],
        [ "Step 5: E3 INDICATION MESSAGE, cell configuration", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md634", null ],
        [ "Step 6: E3 INDICATION MESSAGE, sensing", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md635", null ],
        [ "Step 7: E3 dApp REPORT", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md636", null ],
        [ "Step 8: E3 xApp CONTROL ACTION", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md637", null ],
        [ "Step 9: E3 dApp CONTROL ACTION", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md638", null ],
        [ "Step 10: E3 MESSAGE ACK", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md639", null ]
      ] ],
      [ "Apply outcome", "md_docs_2e3sm_2spectrum_207-examples.html#autotoc_md640", null ]
    ] ],
    [ "E3SM-SPEC: integrator guide", "md_docs_2e3sm_2spectrum_2integrator-guide.html", [
      [ "Where the code references point", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md642", null ],
      [ "Artifacts", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md643", null ],
      [ "The RAN side", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md644", [
        [ "Register the service model", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md645", null ],
        [ "Telemetry", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md646", null ],
        [ "Controls", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md647", null ],
        [ "The report and the xApp control", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md648", null ],
        [ "The apply outcome", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md649", null ]
      ] ],
      [ "The dApp side", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md650", null ],
      [ "The xApp side", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md651", null ],
      [ "Evidence for the discrepancies", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md652", null ],
      [ "Tests", "md_docs_2e3sm_2spectrum_2integrator-guide.html#autotoc_md653", null ]
    ] ],
    [ "Interoperability", "md_docs_2interop.html", [
      [ "What pairs with what", "md_docs_2interop.html#autotoc_md669", null ],
      [ "JSON as libe3 speaks it", "md_docs_2interop.html#autotoc_md670", null ],
      [ "Field contracts", "md_docs_2interop.html#autotoc_md671", null ],
      [ "RAN function ids", "md_docs_2interop.html#autotoc_md672", null ],
      [ "Conformance fixtures", "md_docs_2interop.html#autotoc_md673", null ],
      [ "When the RAN goes away", "md_docs_2interop.html#autotoc_md674", null ],
      [ "Known differences", "md_docs_2interop.html#autotoc_md675", null ],
      [ "Found while writing the specifications", "md_docs_2interop.html#autotoc_md676", [
        [ "E3AP", "md_docs_2interop.html#autotoc_md677", null ],
        [ "E3 service models", "md_docs_2interop.html#autotoc_md678", null ],
        [ "E2SM-DAPP", "md_docs_2interop.html#autotoc_md679", null ]
      ] ],
      [ "Aerial: what to change to align", "md_docs_2interop.html#autotoc_md680", null ],
      [ "OCUDU: what to change to align", "md_docs_2interop.html#autotoc_md681", null ],
      [ "Change ledger", "md_docs_2interop.html#autotoc_md682", null ]
    ] ],
    [ "Latency recording (latrec)", "md_docs_2utilities_2latrec.html", [
      [ "Why a ring, and why one gate", "md_docs_2utilities_2latrec.html#autotoc_md684", [
        [ "Matching the flag downstream", "md_docs_2utilities_2latrec.html#autotoc_md685", null ],
        [ "Python (libe3py)", "md_docs_2utilities_2latrec.html#autotoc_md686", null ],
        [ "Where the rings go", "md_docs_2utilities_2latrec.html#autotoc_md687", null ]
      ] ],
      [ "Clock model", "md_docs_2utilities_2latrec.html#autotoc_md688", null ],
      [ "Ring naming and sizing", "md_docs_2utilities_2latrec.html#autotoc_md689", null ],
      [ "Stage catalog", "md_docs_2utilities_2latrec.html#autotoc_md690", [
        [ "Joining across components", "md_docs_2utilities_2latrec.html#autotoc_md691", null ],
        [ "Attribution, and where the ring name stops being enough", "md_docs_2utilities_2latrec.html#autotoc_md692", [
          [ "Possible follow-up: tier-aware attribution", "md_docs_2utilities_2latrec.html#autotoc_md693", null ]
        ] ],
        [ "Granularity, and adding an identifier", "md_docs_2utilities_2latrec.html#autotoc_md694", null ]
      ] ],
      [ "Capture → CSV workflow", "md_docs_2utilities_2latrec.html#autotoc_md695", null ],
      [ "CI overhead gate", "md_docs_2utilities_2latrec.html#autotoc_md696", null ]
    ] ],
    [ "Contributing", "md_CONTRIBUTING.html", [
      [ "Pull Request Process", "md_CONTRIBUTING.html#autotoc_md728", null ],
      [ "Development Workflow", "md_CONTRIBUTING.html#autotoc_md729", [
        [ "For maintainers and existing collaborators", "md_CONTRIBUTING.html#autotoc_md730", null ],
        [ "For external contributors", "md_CONTRIBUTING.html#autotoc_md731", null ],
        [ "Merging (maintainers)", "md_CONTRIBUTING.html#autotoc_md732", null ]
      ] ],
      [ "Mandatory checks", "md_CONTRIBUTING.html#autotoc_md733", [
        [ "Issues", "md_CONTRIBUTING.html#autotoc_md734", null ],
        [ "Pull requests", "md_CONTRIBUTING.html#autotoc_md735", null ],
        [ "Licensing headers", "md_CONTRIBUTING.html#autotoc_md736", null ],
        [ "AI assistants", "md_CONTRIBUTING.html#autotoc_md737", null ],
        [ "Service Models and twin-repo coordination", "md_CONTRIBUTING.html#autotoc_md738", null ],
        [ "Specifications and their Word editions", "md_CONTRIBUTING.html#autotoc_md739", null ]
      ] ],
      [ "Code of Conduct", "md_CONTRIBUTING.html#autotoc_md740", [
        [ "Our Pledge", "md_CONTRIBUTING.html#autotoc_md741", null ],
        [ "Our Standards", "md_CONTRIBUTING.html#autotoc_md742", null ],
        [ "Our Responsibilities", "md_CONTRIBUTING.html#autotoc_md743", null ],
        [ "Scope", "md_CONTRIBUTING.html#autotoc_md744", null ],
        [ "Enforcement", "md_CONTRIBUTING.html#autotoc_md745", null ],
        [ "Attribution", "md_CONTRIBUTING.html#autotoc_md746", null ]
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
"classlibe3_1_1E3Agent.html#af4eeb30804a78f9c0c52f2f7b748c42d",
"classlibe3_1_1LogStream.html#a2a4a53729988364f1694d1097015dc74",
"classlibe3_1_1ZmqE3Connector.html#aa4faf908ab702dfef5414d98d0c6a305",
"error__codes_8h.html#af5d631a058ca5b6d6166e0651e5bf9f8acd1fbc9bc4e6436f38e5173ca2d2eb40",
"md_docs_2control-loops_2path-b-e2-e3-loop.html#autotoc_md20",
"md_docs_2e3ap_203-procedure-flow.html#autotoc_md285",
"md_docs_2e3sm_2spectrum_203-procedure-flow.html#autotoc_md566",
"namespacelibe3_1_1py.html#a53a0cad66e191bf7ea9824cfc9029917",
"structlibe3_1_1EncodedMessage.html#ae54b303588df3be7014174df81d36beb",
"structlibe3_1_1e2sm__dapp_1_1e2sm__dapp__ctrl__hdr__format1__s.html#ac2d4177cce59443db4f721318fd5ab9d",
"structlibe3_1_1e2sm__dapp_1_1e2sm__dapp__ind__hdr__s.html#a57190da8e2eff729524b1b2336c5d67a",
"structlibe3_1_1py_1_1E3Event.html#a3289640b851ae9089cc92acc0f436f65"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
var LISTOFALLMEMBERS = 'List of all members';