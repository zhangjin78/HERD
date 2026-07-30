#!/usr/bin/env python
import time
import sys
import os
from urllib.request import urlopen
from urllib.error import URLError, HTTPError
import socket




def reconargs():
  import argparse
  p = argparse.ArgumentParser(add_help=False)
  p.add_argument("--geometry",   help='geometry file path',  required = False)
  p.add_argument("--seq",        help='continue to play with n seconds')
  p.add_argument("--input",      help='input file(s)')
  p.add_argument("--entry",      help='entry of the file')
  p.add_argument("--ip",         help='ip address for display server')
  p.add_argument("--port",           help='port for display server')
  p.add_argument("--shorturl",       help='short url for complex url merging ')
  p.add_argument("--calopca",    help='enable calo PCA',     action='store_const', const=True)
  p.add_argument("--fitcluster", help='enable FIT cluster',  action='store_const', const=True)
  p.add_argument("--scdcluster", help='enable SCD cluster',  action='store_const', const=True)
  p.add_argument("--psdhit",     help='enable PSD hit',      action='store_const', const=True)
  p.add_argument("--globaltrack",help='enable Global track', action='store_const', const=True)
  args = p.parse_args()
  return args


class Event():

  def setargs(self, args):
    self.pars = args
    self.time = time.time()
    self.url = ""

  def run_geom(self,):

    from SimConfiger import sniperplus as sp
    sp.setLogLevel(sp.INFO)
    sp.setColorful(sp.WARNING)
    sp.setShowTime(True)
    task = sp.Task("MigrateTest")


    import ProcessManageSvc
    task.createSvc("ProcessManageSvc")


    import PodioDataSvc
    dsvc = task.createSvc("PodioDataSvc")

    if self.pars.input:
      import PodioSvc
      isvc = task.createSvc("PodioInputSvc/InputSvc")
      isvc.property("InputFile").set( self.pars.input)  

    xmlpath = os.path.join(
      os.environ['HERDOS_INSTALL'], 'compact/v2025a/v2025a-scdX.xml'
    )
    if self.pars.geometry:
      if os.path.isabs(self.pars.geometry):
        xmlpath = self.pars.geometry
      else:
        xmlpath = os.path.join(os.environ['HERDOS_INSTALL'], self.pars.geometry)
    else: # what else?
      from SimConfiger.herdos import prodinfo
      pi = prodinfo.ProdInfo.loadFromFile(self.pars.input)
      xmlpath = pi.geofile()
      xmlpath = os.environ['HERDOS_INSTALL'] + '/compact' + xmlpath.split('/compact', 1)[1]

    print("xml path is ", xmlpath)
    geosvc = task.createSvc("GeometrySvc")
    geosvc.setprop("GeoCompactFileName", xmlpath)

    import GlobalTrack
    upsvc = task.createSvc("HitUpdateTool", sp.WARNING)
    import ZDisplay
    dissvc = task.createSvc("WebSocketSvc")
    disalg = task.createAlg("ZDisplayAlg")
    if self.pars.entry:
      disalg.property("entry").set(self.pars.entry)
    else:
      disalg.property("entry").set("0")

    if self.pars.seq:
      disalg.property("continue").set( self.pars.seq )
    else:
      disalg.property("continue").set( "0" )

    if self.pars.ip:
      disalg.property("ip").set( self.pars.ip )

    if self.pars.port:
      disalg.property("port").set( self.pars.port )
      dissvc.property("ws_port").set( self.pars.port )
    if self.pars.shorturl:
      disalg.property("shorturl").set( self.pars.shorturl )
      dissvc.property("ws_path").set( self.pars.shorturl+"/ws/cpp" )


    if self.pars.input:
      task.setEvtMax(-1)
    else:
      task.setEvtMax(0)

    task.show()
    task.run()

  def is_url_available(self, url, timeout=1):
    try:
      print("checking " + url)
      response = urlopen(url, timeout=timeout)
      print(url, response.getcode())
      if response.getcode() == 500:
        return True
      else:
        return False 
    except HTTPError as e:
      return True 
    except URLError as e:
      return False
    except socket.timeout:
      return False
    except Exception as e:
      return False
    return False


  def dispatch(self, event):
    print("display defaults ")
    if time.time() - self.time < 2:
      return
    self.run_geom()


def WatchIt(pars ):
  event_handler = Event()
  event_handler.setargs(pars)
  event_handler.run_geom()



if __name__ == '__main__':
  pars = reconargs();
  WatchIt(pars)
