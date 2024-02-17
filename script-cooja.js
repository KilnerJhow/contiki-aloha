load("nashorn:mozilla_compat.js");

importPackage(java.io);

WAIT_UNTIL(id == 1 && msg.contains("Starting to sense"));

outputs = new Object();
runId = "1-";
fileId = "5-duty-cycle";

// In milliseconds.
TIMEOUT(100000);

while (true) {
  //Has the output file been created.
  if (id == 1 && !msg.contains("Starting to sense")) {
    if (!outputs[id.toString()]) {
      // Open log_<id>.txt for writing.
      // BTW: FileWriter seems to be buffered.
      outputs[id.toString()] = new FileWriter("../../../csv/log_" + runId + fileId + ".csv");
      outputs[id.toString()].write("time,_Eihop,_P0,hops,d,_R,_Nb\n");
    }
    //Write to file.
    outputs[id.toString()].write(time + "," + msg + "\n");
    log.log(time + "," + msg + "\n");
  }

  try {
    //This is the tricky part. The Script is terminated using
    // an exception. This needs to be caught.
    YIELD();
  } catch (e) {
    //Close files.
    for (var ids in outputs) {
      outputs[ids].close();
    }
    //Rethrow exception again, to end the script.
    throw ('test script killed');
  }
}