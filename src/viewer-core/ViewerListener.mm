//
//  ViewerListener.mm
//  avida/apps/viewer-macos
//
//  Created by David M. Bryson on 11/11/10.
//  Copyright 2010-2011 Michigan State University. All rights reserved.
//  http://avida.devosoft.org/viewer-macos
//
//  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
//  following conditions are met:
//  
//  1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the
//      following disclaimer.
//  2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
//      following disclaimer in the documentation and/or other materials provided with the distribution.
//  3.  Neither the name of Michigan State University, nor the names of contributors may be used to endorse or promote
//      products derived from this software without specific prior written permission.
//  
//  THIS SOFTWARE IS PROVIDED BY MICHIGAN STATE UNIVERSITY AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
//  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL MICHIGAN STATE UNIVERSITY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
//  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
//  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//  Authors: David M. Bryson <david@programerror.com>
//

#include "ViewerListener.h"

#include <cassert>


@implementation ViewerMap
- (id) initWithMap:(Avida::Viewer::Map*)map {
  m_map = map;
  return self;
}
@synthesize map = m_map;
@end


@implementation ViewerUpdate
- (id) initWithUpdate:(int)update {
  m_update = update;
  return self;
}
@synthesize update = m_update;
@end


void MainThreadListener::NotifyMap(Avida::Viewer::Map* map)
{
  if ([m_target respondsToSelector:@selector(handleMap:)]) {
    ViewerMap* cvm = [[ViewerMap alloc] initWithMap:map];
    [m_target performSelectorOnMainThread:@selector(handleMap:) withObject:cvm waitUntilDone:NO];
  }
}

void MainThreadListener::NotifyState(Avida::Viewer::DriverPauseState state)
{
  switch (state) {
    case Avida::Viewer::DRIVER_PAUSED:
      if ([m_target respondsToSelector:@selector(handleRunPaused:)]) {
        [m_target performSelectorOnMainThread:@selector(handleRunPaused:) withObject:nil waitUntilDone:NO];
      }
      break;
    case Avida::Viewer::DRIVER_SYNCING:
      [m_target performSelectorOnMainThread:@selector(handleRunSync:) withObject:nil waitUntilDone:NO];
      break;
    case Avida::Viewer::DRIVER_UNPAUSED:
      break;
  }
}

void MainThreadListener::NotifyUpdate(int update)
{
  if ([m_target respondsToSelector:@selector(handleUpdate:)]) {
    ViewerUpdate* cvu = [[ViewerUpdate alloc] initWithUpdate:update];
    [m_target performSelectorOnMainThread:@selector(handleUpdate:) withObject:cvu waitUntilDone:NO];
  }
}
