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
