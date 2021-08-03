/*
Copyright (C) 2021 Intel Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions
and limitations under the License.


SPDX-License-Identifier: Apache-2.0

Author: Xue Yifei (yifei.xue@intel.com)
Date: 2021.06.09

*/

#ifndef __LOCAL_DISPLAY_H__
#define __LOCAL_DISPLAY_H__

int getResFromFb(int& w, int& h);
int getResFromDebugFs(int& w, int& h);
int getResFromKms(int& w, int& h);

#endif
