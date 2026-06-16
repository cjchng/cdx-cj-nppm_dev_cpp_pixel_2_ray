# Session Prompts

This file records the user prompts from the current session in chronological order.

1. `Please review codes of this repo & write a user manual`
2. `Please review the code & suggest how & where to initilize m_state for the widget instance.`
3. `Please show codes to situate the running window. I don't like it overlap with CLion IDE.`
4. `Set the open window to another screen of tft if valid.`
5. `list screen names of this host`
6. `Yes, please!`
7. `The line contures are too narrow`
8. `constexpr float kContourLineWidth = 6.4f;`
   `constexpr float kActiveContourLineWidth = 8.4f;`
   `I cannot find any wider?`
9. `Please!`
10. `Please write a note to describe how to adjust the line width & also review why not worked well at first.`
11. `Please ajust the globe radian lengrh to 1.0`
12. `Please add the operation directive discription on the right panel. such as modify "zoom: 0.48" to "zoom: 0.48  [middle wheell scroll]"`
13. `Yes, please!`
14. `Please adjust the width of right panel to 1/3 of the viewport and adjust the fontsize accordingly, i.e. be larger.`
15. `Please fix the end of negtive z to center of the globe,i.e. lenght=1.0,  and cannot modify anymore.`
16. `Please ajust the cylder suface to inresect with the end point of the positive_z axis, i.e. with radian of 1+the_length_of_positive_z.`
17. `please change color of the cylindrical wall to a color with a better contrast to that on the global surface.`
18. `Please make the mouse a left-button tracking to adjust the local corordinte frame on the globe surface.`
19. `Please update the USER_MANUAL.md`
20. `Please change the both cylinderical heights equal to "sqrt((postive_z+1) * (positive_z+1) + positive_y*positive_y))" and  "sqrt ((postive_z+1) * (positive_z+1) + negtive_y*negtive_y))".`
21. `Please note, The code was updated by me.`
    `const float positiveHeight =  positiveY * xCylinderRadius;`
    `const float negativeHeight = negativeY  * xCylinderRadius;`
22. `Please update USER_MANUAL.md`
23. `The both x & y axial guiding static traces are too long and too strong.`
    `Please dimmer the static framed guiding tracings of the postive_x, the negatuive_x, the positive_y, & the negative_y.`
24. `Can you recall all my prompts for this session to a markdown file`
25. `please accompany a radio check for each editable field to temporary lock values in the editable field.`
    `From now on, please update the SESSION_PROMPTS.md as well for my prompt.`
26. `Please check the implementation of operations relate to the global operation`
27. `Please explain more about " High: move mode corrupts sphere shading and front/back wireframe classification."`
28. `(1) Please git a recovey tag here & document it.`
    `(2) Please modify as your recommendation, and give me a detail note for this modification.`
29. `Please document your previous dialog and your fixed proposal or the global center policy. Then,`
    `I like to have a formal commit here. Please guide me how to do it step by step & I can easily checkout and recover the current state.`
30. `Please do as your guiding notes.`
31. `Please do a remote push`
32. `Please review sequces to draw the viewport`
33. `Please write a note for the last dialog.`
34. `Please modify codes according to your recommendation.`
35. `Please give operating hint for each status on the right panel, such as how to    "move" center, how to o/off "Y Trace", & on/off X cylinder`
36. `Not worked. Please check, and extend "ESC" to quit to the panel`
37. `Pleae keep right cliked pull down, and extend statuses Center, Y Trace, X Cylinder to toggle switching on the panel.`
38. `Please display the terminology we just named in the right panel using an appropriate UI.`
39. `Please update SESION_PROMPT`
40. `Please show  components of the globe coordinate values similar to that for the local coordiante syetem on the right panel. Such as global radian lenght, and ifnp position.`
41. `What is Anchor X, Y, Z?`
42. `Please make the global Radius maiputable`
43. `Please make Global Radius default locked to 1.00 as others.`
44. `Please do the same UI to to camera posing, i.e. parameter adjustable for gluLookAt()`
45. `please save the setting on the right panels, and add a (default or last-run) toogle button to control the program initialization.`
46. `How to adjust camera posing?`
47. `Save a note for the last dialog, then implement the "cameraPosition() / lookAt(...)" at right panel`
48. `It shoud continuously can toggle betwen current state & defauly while running, i.e. don't update default state to the running default.`
49. `Not as expection. Please extent a sibling column for the right panel to show the default.`
50. `Please try to use multiple colums table to put each field aligned row-wisely.`
51. `Please add a common toggle botten to switch between the default & the current`
52. `Suppose we save two modes as saved and default. Using saved mode to initialize the app. Then we don't need the initilization status field.`
53. `Remove the [Ststus/Runtime] and Replaced by clicking the labels [Current] or [Default] in the Row 'Field", change the activated one to a brighter font attribute.`
54. `Fixed the globe center, please implement the orientation adjustment by tracking end points of any global axis.`
55. `The global-axis end picking is hard to distiguguish from the picking of the local component handler. Please modify the global-axis end picking to the combination of the "shift-hold" while the global endpoint picking.`
56. `Not worked the global end points pickng. Please filter out the local left-click event while shift key is  hold.`
57. `The shift issue is solved. Please implement the orientation tracking of the globe.`
58. `Not worked as my expection. Please note we need rotate the globe. Please review the differnce between X-Y-Z axes and x-y-z axes.`
59. `Please write a note for this topic, included the will-be refactoring codes, then, refactor.`
60. `The globe rotates well. But graphic traces of global axes were not following the tracking.`
61. `(1)(Field/Default) can init well. But, stuck the global end tracking after a local opeartion, however, at this momement, the local operation is well-kept.  (2) The (Field/Current) init &  worked in local tracking, but stuck for global end tracking.`
62. `it still stuck. Please check once more.`
63. `(Field-Default) will do globe axes end-tracking once, then, stuck. How ever, the local tracking worked continuously.`
64. `Not any change or improvement found. Please implement the global UI first in right panel.`
65. `I find the global tracking works while q_w=1, q_x=qy=qz=0.`
66. `It worked well. Please use a different texture for back face of the cylinder. Please check static labels shown on the panel, such as [Y-Axis Great Circle] could be [y-Axis ...] and [X-Axis Tangen...} could be [x-Axis Tangen...]`
67. `Please toggle enable/disable the depth hiding for globe & cylinder by a right click pull down selection.`
68. `Please extend camera projection models option on the right panel`
69. `Where do we save default?`
70. `Do not hard code, please export the default & current status to their respective json files.`
71. `Please add symbol links of the json files at the working root.`
72. `can we use yaml files?`
73. `What is "Surface Depth Hiding" status?`
74. `I cannot see the effect for the cylinder.`
75. `Write a note for this issue, then, implement your suggestion.`
76. `Please disable  using translucent fill for cylinder.`
77. `The "Entaniya 280-Facing Up.jpg" is a equid-solid projection image with "Cap Angle" 140 degree. Please map the image to suface of the globe.`
78. `Please extract the circular border to situate of the  valid image region, then do the mapping accordinly.`
79. `Please improve the mapping resolution`
80. `Please enhace 4 times finer.`
81. `please improve it by an advanced interpolation.`
82. `please map the circular.png`
83. `the next upgrade would be bicubic interpolation or mip-style prefiltering.`
84. `Please implement a command menu that own a tab to set a file of fisheye image to be mapped to globe,`
85. `please`
86. `Bug message:`
87. `/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/cmake-build-debug/pixel_2_ray_qt`
88. `2026-05-25 23:06:41.564 pixel_2_ray_qt[19905:30106866] not in fullscreen state`
89. `2026-05-25 23:06:41.564 pixel_2_ray_qt[19905:30106866] not in fullscreen state`
90. `Please document this issue.`
91. `(1)please use circular.png as the default init. (2)If cylinder is on, please contruct the texture on the cylinder suface by inverse mapping to a pixel on the global suface, if of sub-pixel please use the nearest one.`
92. `/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/cmake-build-debug/pixel_2_ray_qt`
93. `2026-05-25 23:28:12.372 pixel_2_ray_qt[20453:30126066] not in fullscreen state`
94. `2026-05-25 23:28:12.373 pixel_2_ray_qt[20453:30126066] not in fullscreen state`
95. `/Users/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/cmake-build-debug/pixel_2_ray_qt`
96. `2026-05-25 23:30:09.695 pixel_2_ray_qt[20637:30129211] not in fullscreen state`
97. `2026-05-25 23:30:09.695 pixel_2_ray_qt[20637:30129211] not in fullscreen state`
98. `Please make global traces with more visual contast difference from local axes.`
99. `Please!`
100. `I suggest also in terminal shapes for end ponts`
101. `Do not change  the origin of the local origin by a simple clik. It must be through a hold-tracking to its origin.  Bytheways, please make the origin shape more distigushable from other axial end points.`
102. `Please revise the operation manual, readme, session prompts & other documents, before a new commit.`
103. `Please mofify the location of the default_state.json & current_state.json to assets folder`
104. `Please always show the output screen "Pixel To Ray QT Window. on the leftest window`
105. `Extend the running configuration function on the right panel to an additinal browser based UI Python Server. Please keep the both UI at this stage.`
106. `Please truncate numerical format to not more than 2 digit after digit point in the right panel or the config_ui_server.`
107. `Please attach the Locks to their repected field for the compactness of the UI server`
108. `Please show the equivent 4-compnents Euler angles below the 4-componets quaternion of the Global Orientaion. Draw a contrast delimiter between the two rotation system and updated sychronously online.`
109. `Please document the current implementation of Global Orientation module`
110. `Please explain in detail the tranfer from [Global q_?] to "Euler axis_?"  "GLOBAL_ORIENTATION_REFACTOR_NOTES.md", and explain its relation ship to roo-yaw-pitch`
111. `+Despite the panel label, this is not a yaw/pitch/roll triplet. It is the single rotation axis plus the single rotation amount represented by the same quaternion.`
112. `Let's refactor "CYLINDER_SHADER_TEXTURE_PLAN.md"`
113. `Please enhance files explorer functions to the imgage panel of the ui_server`
114. `Let refactor "CYLINDER_SHADER_TEXTURE_PLAN" step by step`
115. `Go!`
116. `Next step please!`
117. `Go!`
118. `Please GO!`
119. `Let's GO`
120. `Please extend Visibility Panel of UI SERVER to include all funtions in right cliked pop-up.`
121. `"Rear Hemisphere Wireframe" is not worked as the right-pop, please check!`
122. `Please refactor UI server to show Current and Default on differnt columns of a united panel.`
123. `Width of numerical fields are too narrow for those with a Lock check box.  Please refactor to use an alternative toggle locker icon.`
124. `Please widen space for numerical fields with LOCKER.`
125. `Bug: The UI Server cannot switching between "Current" & "Default"`
126. `Please document the collaboration between the application and the UI server. We found that switching between "Current" mode and "Default" mode is not synchronized.`
127. `Please post the IP:Port in app title`
128. `Please refactor to synchronize "Current" or "Default"  trigger by Right Panel on the APP or by UI Server.`
129. `Add a new button to save the "Current" with a new name`
130. `Please add a "Load As Current"`
131. `Please label the name of the current profile`
132. `Please refactor to file browser UI widgets for "Save Current As" & "Load As Current"`
133. `Please add a delete with re-confirmation for the "Saved Profile Files"`
134. `Please add rename with confirmation`
135. `Refactor "Rename": While button "Rename" is cliked, the first processing step is to transfer the "Current" to the selected one in "Saved Profile Files".`
136. `Extend "Rename" to two-pass confirmation, i.e. first: "Update Current" to the Selected", second: "Confirm Rename".`
137. `Please change Current to the selected after the first confirmation.`
138. `Error: While "Confirm Rename" is shown, the "File name" should be editable(missing state), before the second confirmation.`
139. `Please extend "[World axes] enable/disable" in [Visibilty] for UI Server`
140. `Refactor: Activate [Load As Current] to the Clicked (File Name) in [Saved Profile Files]`
141. `UI Server: Extend a file open widget to update the (Current) in [Current Profile Snapshots]`
142. `Please move [image] to between [Current Profille Snapshot] & [View]`
143. `Please extend an Editable zone where the user can keep the personal comment in [Current Profile Snapshots].`
144. `  the string label [Personal comment] to [PERSONAL COMMENT; click BTN(Save Current As) if updated]`
145. `Please change the string to flash red warning color if being edited`
146. `Please move [Perspective Projection] to the first row of Panel [Globe Orientation] of UI Server`
147. `Please extend a [Cap Closed] in [Visibility]`
148. `Please Check (the disable state) of [Local Coordinate Frame] [Visibilty] to clean (the rsidued local origin icon) & (the residued trace to the global origin)`
149. `Please interchange the panel [View] with [Visibility]`
150. `Please add [Local Components] a row,named [va], to equate [positive_y] in term of the angle constructed with local_z axis taking global center as the intersection`
151. `Please extend the [Locker] option for [Globe Orientation]`
152. `Please document fields of [Camera]`
153. `Please explain and revise the User Manual.md : Eye Offset: (x,y):(0.01, 0.0) is an offset rotation of (0.0, 0.01) and is not functional for (0.0, 0.0)`
154. `Screenshots clarify Eye Offset X/Y examples: (0.01,0,-40), (0,0.01,-40), (0.01,0.01,-40), and the singular (0,0,-40) case`
155. `Please put the upload images in the USER_MANUAL.md and explain them in detail.`
156. `Pleae implement [pitch], [yaw], [roll] in [Clobal Orientation] for UI Server`
157. `Refactor [Roll][Yaw][Pitch] as transient Enter-confirmed rotations around (Z)(Y)(X), reset to zero after confirmation, no spinner, subtle UI`
158. `Remove the post-confirmation reset-to-zero behavior from the [Roll][Yaw][Pitch] UI Server controls`
159. `Document why Roll(Z) keeps world Z fixed instead of the rotated local Z axis, and clarify whether fixed rotated axis implies fixed q`
160. `Refactor Roll/Yaw/Pitch command composition to local axes and explicitly center the fixed global center guide on screen`
161. `Clarify whether Visibility [World Axes] should be called [World Axes] or [Globe Axes], then revise and extend documentation`
162. `Refactor UI Server so Save Current As refreshes both Saved Profile Files and Open Profile File file lists`
163. `I will simplfy the UI. Please integrate [Open Profile Files] with [Save Profile File], and merge the both to [Profile File]`
164. `Is [Open As Current] & [Load As Current] redundant?`
165. `Why do we keep two internally? Can we remove the "Load as ..." completely?`
166. `Please refactor [rename] to the pattern as the regular file browser, i.e to remove the 2-step confirmation.`
167. `Can we merge [Load As Current] and [Load As Current] to [open] now?`
168. `Can refactor [save] with confirmation to replace [Profile name] module, including the (file name label) & [Save Current As]?`
169. `Can we change [Save] to [Save|Rename] to merge [Save] & [Rename]?`
170. `Please don't ask my confirmation for "kill UI Serve Process" or some un-harm regular propmts. Please Go.`
171. `What is the stale process? Why you worry?`
172. `Please kill & restart automatically from now on in this session.`
173. `Why you ask, dangerous?`
174. `Go!`
175. `Refactor [Delete]: Don't Switch (Current) to the 1st file. Please switch to the one above or near the deleted.`
176. `Please change the font color of the selected file on the table listing filenames`
177. `Screenshot note: selected saved-profile row color is not distinguishable; please flash the selected row continuously.`
178. `Refactor: Do not flash the text.`
179. `Do not flash the backgound.`
180. `Can we make it editable in order to replace [Save|Rename]`
181. `Please enable keys up/down to switch the selection`
182. `What does [Open] do?`
183. `Move [Perspective Projection] to the first row of [Camera]`
184. `Move [Global Radius] to first row of [Globe Orientation]`
185. `If you select [Preset], the fields that dedicatively belong to [Current] will be dimmed or disabled, and vice versa.`
186. `Refactor: Change [Default] to [Preset], then check contents in the markdown file, & filename repo/asset/default_state.json`
187. `Screenshot note: adjust the rendered image to the center of the screen.`
188. `In separted documents, (1) Please document this dialog  (2)Please document the dialog about refactor [Default] to [Preset]`
189. `Refactor: Make two [Profile File]es distinguishable.`
190. `Move [Current:...] to [Profile File Manager]`
191. `Do we need [File name for Save or Rename]?`
192. `Why "Keep the input"?`
193. `Do we need [Current save File] ?`
194. `Yes`
195. `(1) Extend "save" attribute to sub-dirctories of asset/saved_profiles; (2)Extend the editable file name include the path from asset for [SAVE]; (3) Remove label below, [Dirctory:...]`
196. `Please update 'SESSION_PROMPTS.md'`
197. `(2)Extend the label of the editable file name to that includes the relative path from asset dirctory on for [SAVE]`
198. `(1) move module [PERSONAL COMMENT] below module [Profile File Browser]; [2] typo, Rename to [Profile Files Browser]`
199. `Using 'SAVED" as an attribute in our implementation is easy to confuse with a normal file that has been saved.  Please suggest a more distinctive term for this app.`
200. `Please refactor as your recommendation and revise documents`
201. `Pleas enhance contrast visibility of the Readonly String [Current:??] for the fisrt row of [Profile Manager].`
202. `It is quite confused that the editable string of [Save from asset/] will be updated to show the selected, then, a moment later will be recovered to its original, i.e. [Current]. I found this editable field is a worked bridge to assign [Current] to another sublayer of asset/saved_profiles. However, the unknow switching will make users unconfortable.`
203. `Refactor the label [SAVE from asset/] to reflect its real intent.`
204. `Refactor: Split the files list explorer into two columns in order to enhance observability`
205. `Refactor:(1) [Save Current to asset] to [Save to assets/saved_profile/] & delete the prefix substring "saved_profiles" for the editable string. (2) Also similar treatment to the selected one in the [Profile Files Browser]`
206. `Refactor: Panel [image] currently process [current][Preset], please keep only the activated one & remove the other one.`
207. `Refactor images list explorer to single column`
208. `Move the Iconic Image up to the row below the [Fisheye Image Path]`
209. `Show the abosolute path/file from layer assets on`
210. `(1) Remove static labels [Absolute Image File:....] & [Asset layer path: ....].`
211. `(1) Refact string [Fisheye Image Path] to [Fishey Image Assets/hfc-images/];`
212. `(1)Refactor string [Fisheye Image Assets/hfc-images] to [Assets/hfc-images]`
213. `refactor the absolute paths of [Asset/hfc-images] to the relative path below hfc-images`
214. `Pleas write a document how to implement panel [Image] as panel [Profile File Manager] to show the first colume from (image) to (hfc), later, the app would confine its own image domain`
215. `Please follow the [Profile...] pattern to implement [Image], including [rename], [Open], and etc..`
216. `Show contents of Key Changes in a dynamic editable whidget when cliked the title zone of the panel [Image],Then users are free to modify the text before another cliking to a new update and disappear. The saved should appear for next updating phase. If possible simulate vi editor.`
217. `[Image #1] UI Server screenshot showing client-side error: selectedEntryProfilePath is not defined.`
218. `Please implement [Save] for [Image]`
219. `Please refactor one of the two [Open]es to [OpenDir] for [Image]`
220. `Please implement the right panel [Control Table] of APP to Tabs pulldown on the Commands Bar.`
221. `[Image #1] Screenshot shows the Control Tab pulldown at the far right of the menu bar with low-contrast label.`
222. `[Image #1] Change each combo-pull selectable to a tab on the menu bar after the [Command] tab.`
223. `[Image #1] The menu bar still shows only Commands; direct section tabs are not visible as described.`
224. `Please remove the always-shown right panel and use the popular regular dynamic overlay panel to process the selected tab.`
225. `Please emulate the font size adjustment as Chrome, i.e. [Cotrol plus] [Contol minus]`
226. `(1) Move the dynamic panel to the left-top of the panel. (2) Config a quick key for each TAB to activate the dynamic panel, following the legend annotation to [COMMAND]`
227. `[1] Adjust the scale of (the panel) based on the fontsize. [2] Activate (the panel) while fouseing the TAB. then move cursor focusing (the panel) to do the needed adjustment [3] Hide (the panel) while cursor moveout, defocus.`
228. `Please make the fontsize pesistent`
229. `Always make the tail node visible for the editable string of the filepath of [OpenDir][Image][UI SERVER], i.e., preventing the hiding of the string end because there are too many sublayers from the root.`
230. `[Image #1] Screenshot shows the Image path row and Image OpenDir field still clipping the rightmost path tail.`
231. `I suppose we can remove the second row of [Image]`
232. `[Image #1] UI Server Image panel screenshot: second row still visible; ask whether removable and why Delete is red.`
233. `Can [UI SERVER] shrink and expand the select panel dynamically?`
234. `Can [Euler axis-angle equivalent|read-only] & [Local-axis rotation | ...] cllapse?`
235. `Please document the mapping of fisheye image to globe surface. [1] if we have different projections, [2] if we have pixel to spatial ray discrete mapping, [3] if we want horizontal flip or vertical flip before loading. Please make a plan document for our next step developing.`
236. `Make [Profile File Manage] collapse-able`
237. `How to implement and test your Example`
238. `Please document this automatic record/playback issue including to dissect 'tools/action*.py" in detail`
