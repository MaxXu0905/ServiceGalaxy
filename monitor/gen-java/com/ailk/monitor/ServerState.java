/**
 * Autogenerated by Thrift Compiler (0.8.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
package com.ailk.monitor;

import org.apache.thrift.scheme.IScheme;
import org.apache.thrift.scheme.SchemeFactory;
import org.apache.thrift.scheme.StandardScheme;

import org.apache.thrift.scheme.TupleScheme;
import org.apache.thrift.protocol.TTupleProtocol;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.EnumMap;
import java.util.Set;
import java.util.HashSet;
import java.util.EnumSet;
import java.util.Collections;
import java.util.BitSet;
import java.nio.ByteBuffer;
import java.util.Arrays;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class ServerState implements org.apache.thrift.TBase<ServerState, ServerState._Fields>, java.io.Serializable, Cloneable {
  private static final org.apache.thrift.protocol.TStruct STRUCT_DESC = new org.apache.thrift.protocol.TStruct("ServerState");

  private static final org.apache.thrift.protocol.TField SGID_FIELD_DESC = new org.apache.thrift.protocol.TField("sgid", org.apache.thrift.protocol.TType.I64, (short)1);
  private static final org.apache.thrift.protocol.TField SERVERID_FIELD_DESC = new org.apache.thrift.protocol.TField("serverid", org.apache.thrift.protocol.TType.I32, (short)2);
  private static final org.apache.thrift.protocol.TField TOTAL_IDLE_FIELD_DESC = new org.apache.thrift.protocol.TField("total_idle", org.apache.thrift.protocol.TType.I32, (short)3);
  private static final org.apache.thrift.protocol.TField TOTAL_BUSY_FIELD_DESC = new org.apache.thrift.protocol.TField("total_busy", org.apache.thrift.protocol.TType.I32, (short)4);
  private static final org.apache.thrift.protocol.TField NCOMPLETED_FIELD_DESC = new org.apache.thrift.protocol.TField("ncompleted", org.apache.thrift.protocol.TType.I64, (short)5);
  private static final org.apache.thrift.protocol.TField WKCOMPLETED_FIELD_DESC = new org.apache.thrift.protocol.TField("wkcompleted", org.apache.thrift.protocol.TType.I64, (short)6);
  private static final org.apache.thrift.protocol.TField CTIME_FIELD_DESC = new org.apache.thrift.protocol.TField("ctime", org.apache.thrift.protocol.TType.I64, (short)7);
  private static final org.apache.thrift.protocol.TField STATUS_FIELD_DESC = new org.apache.thrift.protocol.TField("status", org.apache.thrift.protocol.TType.I32, (short)8);
  private static final org.apache.thrift.protocol.TField MTIME_FIELD_DESC = new org.apache.thrift.protocol.TField("mtime", org.apache.thrift.protocol.TType.I64, (short)9);
  private static final org.apache.thrift.protocol.TField SVRIDMAX_FIELD_DESC = new org.apache.thrift.protocol.TField("svridmax", org.apache.thrift.protocol.TType.I64, (short)10);

  private static final Map<Class<? extends IScheme>, SchemeFactory> schemes = new HashMap<Class<? extends IScheme>, SchemeFactory>();
  static {
    schemes.put(StandardScheme.class, new ServerStateStandardSchemeFactory());
    schemes.put(TupleScheme.class, new ServerStateTupleSchemeFactory());
  }

  public long sgid; // required
  public int serverid; // required
  public int total_idle; // required
  public int total_busy; // required
  public long ncompleted; // required
  public long wkcompleted; // required
  public long ctime; // required
  public int status; // required
  public long mtime; // required
  public long svridmax; // required

  /** The set of fields this struct contains, along with convenience methods for finding and manipulating them. */
  public enum _Fields implements org.apache.thrift.TFieldIdEnum {
    SGID((short)1, "sgid"),
    SERVERID((short)2, "serverid"),
    TOTAL_IDLE((short)3, "total_idle"),
    TOTAL_BUSY((short)4, "total_busy"),
    NCOMPLETED((short)5, "ncompleted"),
    WKCOMPLETED((short)6, "wkcompleted"),
    CTIME((short)7, "ctime"),
    STATUS((short)8, "status"),
    MTIME((short)9, "mtime"),
    SVRIDMAX((short)10, "svridmax");

    private static final Map<String, _Fields> byName = new HashMap<String, _Fields>();

    static {
      for (_Fields field : EnumSet.allOf(_Fields.class)) {
        byName.put(field.getFieldName(), field);
      }
    }

    /**
     * Find the _Fields constant that matches fieldId, or null if its not found.
     */
    public static _Fields findByThriftId(int fieldId) {
      switch(fieldId) {
        case 1: // SGID
          return SGID;
        case 2: // SERVERID
          return SERVERID;
        case 3: // TOTAL_IDLE
          return TOTAL_IDLE;
        case 4: // TOTAL_BUSY
          return TOTAL_BUSY;
        case 5: // NCOMPLETED
          return NCOMPLETED;
        case 6: // WKCOMPLETED
          return WKCOMPLETED;
        case 7: // CTIME
          return CTIME;
        case 8: // STATUS
          return STATUS;
        case 9: // MTIME
          return MTIME;
        case 10: // SVRIDMAX
          return SVRIDMAX;
        default:
          return null;
      }
    }

    /**
     * Find the _Fields constant that matches fieldId, throwing an exception
     * if it is not found.
     */
    public static _Fields findByThriftIdOrThrow(int fieldId) {
      _Fields fields = findByThriftId(fieldId);
      if (fields == null) throw new IllegalArgumentException("Field " + fieldId + " doesn't exist!");
      return fields;
    }

    /**
     * Find the _Fields constant that matches name, or null if its not found.
     */
    public static _Fields findByName(String name) {
      return byName.get(name);
    }

    private final short _thriftId;
    private final String _fieldName;

    _Fields(short thriftId, String fieldName) {
      _thriftId = thriftId;
      _fieldName = fieldName;
    }

    public short getThriftFieldId() {
      return _thriftId;
    }

    public String getFieldName() {
      return _fieldName;
    }
  }

  // isset id assignments
  private static final int __SGID_ISSET_ID = 0;
  private static final int __SERVERID_ISSET_ID = 1;
  private static final int __TOTAL_IDLE_ISSET_ID = 2;
  private static final int __TOTAL_BUSY_ISSET_ID = 3;
  private static final int __NCOMPLETED_ISSET_ID = 4;
  private static final int __WKCOMPLETED_ISSET_ID = 5;
  private static final int __CTIME_ISSET_ID = 6;
  private static final int __STATUS_ISSET_ID = 7;
  private static final int __MTIME_ISSET_ID = 8;
  private static final int __SVRIDMAX_ISSET_ID = 9;
  private BitSet __isset_bit_vector = new BitSet(10);
  public static final Map<_Fields, org.apache.thrift.meta_data.FieldMetaData> metaDataMap;
  static {
    Map<_Fields, org.apache.thrift.meta_data.FieldMetaData> tmpMap = new EnumMap<_Fields, org.apache.thrift.meta_data.FieldMetaData>(_Fields.class);
    tmpMap.put(_Fields.SGID, new org.apache.thrift.meta_data.FieldMetaData("sgid", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I64)));
    tmpMap.put(_Fields.SERVERID, new org.apache.thrift.meta_data.FieldMetaData("serverid", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I32)));
    tmpMap.put(_Fields.TOTAL_IDLE, new org.apache.thrift.meta_data.FieldMetaData("total_idle", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I32)));
    tmpMap.put(_Fields.TOTAL_BUSY, new org.apache.thrift.meta_data.FieldMetaData("total_busy", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I32)));
    tmpMap.put(_Fields.NCOMPLETED, new org.apache.thrift.meta_data.FieldMetaData("ncompleted", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I64)));
    tmpMap.put(_Fields.WKCOMPLETED, new org.apache.thrift.meta_data.FieldMetaData("wkcompleted", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I64)));
    tmpMap.put(_Fields.CTIME, new org.apache.thrift.meta_data.FieldMetaData("ctime", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I64)));
    tmpMap.put(_Fields.STATUS, new org.apache.thrift.meta_data.FieldMetaData("status", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I32)));
    tmpMap.put(_Fields.MTIME, new org.apache.thrift.meta_data.FieldMetaData("mtime", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I64)));
    tmpMap.put(_Fields.SVRIDMAX, new org.apache.thrift.meta_data.FieldMetaData("svridmax", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I64)));
    metaDataMap = Collections.unmodifiableMap(tmpMap);
    org.apache.thrift.meta_data.FieldMetaData.addStructMetaDataMap(ServerState.class, metaDataMap);
  }

  public ServerState() {
  }

  public ServerState(
    long sgid,
    int serverid,
    int total_idle,
    int total_busy,
    long ncompleted,
    long wkcompleted,
    long ctime,
    int status,
    long mtime,
    long svridmax)
  {
    this();
    this.sgid = sgid;
    setSgidIsSet(true);
    this.serverid = serverid;
    setServeridIsSet(true);
    this.total_idle = total_idle;
    setTotal_idleIsSet(true);
    this.total_busy = total_busy;
    setTotal_busyIsSet(true);
    this.ncompleted = ncompleted;
    setNcompletedIsSet(true);
    this.wkcompleted = wkcompleted;
    setWkcompletedIsSet(true);
    this.ctime = ctime;
    setCtimeIsSet(true);
    this.status = status;
    setStatusIsSet(true);
    this.mtime = mtime;
    setMtimeIsSet(true);
    this.svridmax = svridmax;
    setSvridmaxIsSet(true);
  }

  /**
   * Performs a deep copy on <i>other</i>.
   */
  public ServerState(ServerState other) {
    __isset_bit_vector.clear();
    __isset_bit_vector.or(other.__isset_bit_vector);
    this.sgid = other.sgid;
    this.serverid = other.serverid;
    this.total_idle = other.total_idle;
    this.total_busy = other.total_busy;
    this.ncompleted = other.ncompleted;
    this.wkcompleted = other.wkcompleted;
    this.ctime = other.ctime;
    this.status = other.status;
    this.mtime = other.mtime;
    this.svridmax = other.svridmax;
  }

  public ServerState deepCopy() {
    return new ServerState(this);
  }

  @Override
  public void clear() {
    setSgidIsSet(false);
    this.sgid = 0;
    setServeridIsSet(false);
    this.serverid = 0;
    setTotal_idleIsSet(false);
    this.total_idle = 0;
    setTotal_busyIsSet(false);
    this.total_busy = 0;
    setNcompletedIsSet(false);
    this.ncompleted = 0;
    setWkcompletedIsSet(false);
    this.wkcompleted = 0;
    setCtimeIsSet(false);
    this.ctime = 0;
    setStatusIsSet(false);
    this.status = 0;
    setMtimeIsSet(false);
    this.mtime = 0;
    setSvridmaxIsSet(false);
    this.svridmax = 0;
  }

  public long getSgid() {
    return this.sgid;
  }

  public ServerState setSgid(long sgid) {
    this.sgid = sgid;
    setSgidIsSet(true);
    return this;
  }

  public void unsetSgid() {
    __isset_bit_vector.clear(__SGID_ISSET_ID);
  }

  /** Returns true if field sgid is set (has been assigned a value) and false otherwise */
  public boolean isSetSgid() {
    return __isset_bit_vector.get(__SGID_ISSET_ID);
  }

  public void setSgidIsSet(boolean value) {
    __isset_bit_vector.set(__SGID_ISSET_ID, value);
  }

  public int getServerid() {
    return this.serverid;
  }

  public ServerState setServerid(int serverid) {
    this.serverid = serverid;
    setServeridIsSet(true);
    return this;
  }

  public void unsetServerid() {
    __isset_bit_vector.clear(__SERVERID_ISSET_ID);
  }

  /** Returns true if field serverid is set (has been assigned a value) and false otherwise */
  public boolean isSetServerid() {
    return __isset_bit_vector.get(__SERVERID_ISSET_ID);
  }

  public void setServeridIsSet(boolean value) {
    __isset_bit_vector.set(__SERVERID_ISSET_ID, value);
  }

  public int getTotal_idle() {
    return this.total_idle;
  }

  public ServerState setTotal_idle(int total_idle) {
    this.total_idle = total_idle;
    setTotal_idleIsSet(true);
    return this;
  }

  public void unsetTotal_idle() {
    __isset_bit_vector.clear(__TOTAL_IDLE_ISSET_ID);
  }

  /** Returns true if field total_idle is set (has been assigned a value) and false otherwise */
  public boolean isSetTotal_idle() {
    return __isset_bit_vector.get(__TOTAL_IDLE_ISSET_ID);
  }

  public void setTotal_idleIsSet(boolean value) {
    __isset_bit_vector.set(__TOTAL_IDLE_ISSET_ID, value);
  }

  public int getTotal_busy() {
    return this.total_busy;
  }

  public ServerState setTotal_busy(int total_busy) {
    this.total_busy = total_busy;
    setTotal_busyIsSet(true);
    return this;
  }

  public void unsetTotal_busy() {
    __isset_bit_vector.clear(__TOTAL_BUSY_ISSET_ID);
  }

  /** Returns true if field total_busy is set (has been assigned a value) and false otherwise */
  public boolean isSetTotal_busy() {
    return __isset_bit_vector.get(__TOTAL_BUSY_ISSET_ID);
  }

  public void setTotal_busyIsSet(boolean value) {
    __isset_bit_vector.set(__TOTAL_BUSY_ISSET_ID, value);
  }

  public long getNcompleted() {
    return this.ncompleted;
  }

  public ServerState setNcompleted(long ncompleted) {
    this.ncompleted = ncompleted;
    setNcompletedIsSet(true);
    return this;
  }

  public void unsetNcompleted() {
    __isset_bit_vector.clear(__NCOMPLETED_ISSET_ID);
  }

  /** Returns true if field ncompleted is set (has been assigned a value) and false otherwise */
  public boolean isSetNcompleted() {
    return __isset_bit_vector.get(__NCOMPLETED_ISSET_ID);
  }

  public void setNcompletedIsSet(boolean value) {
    __isset_bit_vector.set(__NCOMPLETED_ISSET_ID, value);
  }

  public long getWkcompleted() {
    return this.wkcompleted;
  }

  public ServerState setWkcompleted(long wkcompleted) {
    this.wkcompleted = wkcompleted;
    setWkcompletedIsSet(true);
    return this;
  }

  public void unsetWkcompleted() {
    __isset_bit_vector.clear(__WKCOMPLETED_ISSET_ID);
  }

  /** Returns true if field wkcompleted is set (has been assigned a value) and false otherwise */
  public boolean isSetWkcompleted() {
    return __isset_bit_vector.get(__WKCOMPLETED_ISSET_ID);
  }

  public void setWkcompletedIsSet(boolean value) {
    __isset_bit_vector.set(__WKCOMPLETED_ISSET_ID, value);
  }

  public long getCtime() {
    return this.ctime;
  }

  public ServerState setCtime(long ctime) {
    this.ctime = ctime;
    setCtimeIsSet(true);
    return this;
  }

  public void unsetCtime() {
    __isset_bit_vector.clear(__CTIME_ISSET_ID);
  }

  /** Returns true if field ctime is set (has been assigned a value) and false otherwise */
  public boolean isSetCtime() {
    return __isset_bit_vector.get(__CTIME_ISSET_ID);
  }

  public void setCtimeIsSet(boolean value) {
    __isset_bit_vector.set(__CTIME_ISSET_ID, value);
  }

  public int getStatus() {
    return this.status;
  }

  public ServerState setStatus(int status) {
    this.status = status;
    setStatusIsSet(true);
    return this;
  }

  public void unsetStatus() {
    __isset_bit_vector.clear(__STATUS_ISSET_ID);
  }

  /** Returns true if field status is set (has been assigned a value) and false otherwise */
  public boolean isSetStatus() {
    return __isset_bit_vector.get(__STATUS_ISSET_ID);
  }

  public void setStatusIsSet(boolean value) {
    __isset_bit_vector.set(__STATUS_ISSET_ID, value);
  }

  public long getMtime() {
    return this.mtime;
  }

  public ServerState setMtime(long mtime) {
    this.mtime = mtime;
    setMtimeIsSet(true);
    return this;
  }

  public void unsetMtime() {
    __isset_bit_vector.clear(__MTIME_ISSET_ID);
  }

  /** Returns true if field mtime is set (has been assigned a value) and false otherwise */
  public boolean isSetMtime() {
    return __isset_bit_vector.get(__MTIME_ISSET_ID);
  }

  public void setMtimeIsSet(boolean value) {
    __isset_bit_vector.set(__MTIME_ISSET_ID, value);
  }

  public long getSvridmax() {
    return this.svridmax;
  }

  public ServerState setSvridmax(long svridmax) {
    this.svridmax = svridmax;
    setSvridmaxIsSet(true);
    return this;
  }

  public void unsetSvridmax() {
    __isset_bit_vector.clear(__SVRIDMAX_ISSET_ID);
  }

  /** Returns true if field svridmax is set (has been assigned a value) and false otherwise */
  public boolean isSetSvridmax() {
    return __isset_bit_vector.get(__SVRIDMAX_ISSET_ID);
  }

  public void setSvridmaxIsSet(boolean value) {
    __isset_bit_vector.set(__SVRIDMAX_ISSET_ID, value);
  }

  public void setFieldValue(_Fields field, Object value) {
    switch (field) {
    case SGID:
      if (value == null) {
        unsetSgid();
      } else {
        setSgid((Long)value);
      }
      break;

    case SERVERID:
      if (value == null) {
        unsetServerid();
      } else {
        setServerid((Integer)value);
      }
      break;

    case TOTAL_IDLE:
      if (value == null) {
        unsetTotal_idle();
      } else {
        setTotal_idle((Integer)value);
      }
      break;

    case TOTAL_BUSY:
      if (value == null) {
        unsetTotal_busy();
      } else {
        setTotal_busy((Integer)value);
      }
      break;

    case NCOMPLETED:
      if (value == null) {
        unsetNcompleted();
      } else {
        setNcompleted((Long)value);
      }
      break;

    case WKCOMPLETED:
      if (value == null) {
        unsetWkcompleted();
      } else {
        setWkcompleted((Long)value);
      }
      break;

    case CTIME:
      if (value == null) {
        unsetCtime();
      } else {
        setCtime((Long)value);
      }
      break;

    case STATUS:
      if (value == null) {
        unsetStatus();
      } else {
        setStatus((Integer)value);
      }
      break;

    case MTIME:
      if (value == null) {
        unsetMtime();
      } else {
        setMtime((Long)value);
      }
      break;

    case SVRIDMAX:
      if (value == null) {
        unsetSvridmax();
      } else {
        setSvridmax((Long)value);
      }
      break;

    }
  }

  public Object getFieldValue(_Fields field) {
    switch (field) {
    case SGID:
      return Long.valueOf(getSgid());

    case SERVERID:
      return Integer.valueOf(getServerid());

    case TOTAL_IDLE:
      return Integer.valueOf(getTotal_idle());

    case TOTAL_BUSY:
      return Integer.valueOf(getTotal_busy());

    case NCOMPLETED:
      return Long.valueOf(getNcompleted());

    case WKCOMPLETED:
      return Long.valueOf(getWkcompleted());

    case CTIME:
      return Long.valueOf(getCtime());

    case STATUS:
      return Integer.valueOf(getStatus());

    case MTIME:
      return Long.valueOf(getMtime());

    case SVRIDMAX:
      return Long.valueOf(getSvridmax());

    }
    throw new IllegalStateException();
  }

  /** Returns true if field corresponding to fieldID is set (has been assigned a value) and false otherwise */
  public boolean isSet(_Fields field) {
    if (field == null) {
      throw new IllegalArgumentException();
    }

    switch (field) {
    case SGID:
      return isSetSgid();
    case SERVERID:
      return isSetServerid();
    case TOTAL_IDLE:
      return isSetTotal_idle();
    case TOTAL_BUSY:
      return isSetTotal_busy();
    case NCOMPLETED:
      return isSetNcompleted();
    case WKCOMPLETED:
      return isSetWkcompleted();
    case CTIME:
      return isSetCtime();
    case STATUS:
      return isSetStatus();
    case MTIME:
      return isSetMtime();
    case SVRIDMAX:
      return isSetSvridmax();
    }
    throw new IllegalStateException();
  }

  @Override
  public boolean equals(Object that) {
    if (that == null)
      return false;
    if (that instanceof ServerState)
      return this.equals((ServerState)that);
    return false;
  }

  public boolean equals(ServerState that) {
    if (that == null)
      return false;

    boolean this_present_sgid = true;
    boolean that_present_sgid = true;
    if (this_present_sgid || that_present_sgid) {
      if (!(this_present_sgid && that_present_sgid))
        return false;
      if (this.sgid != that.sgid)
        return false;
    }

    boolean this_present_serverid = true;
    boolean that_present_serverid = true;
    if (this_present_serverid || that_present_serverid) {
      if (!(this_present_serverid && that_present_serverid))
        return false;
      if (this.serverid != that.serverid)
        return false;
    }

    boolean this_present_total_idle = true;
    boolean that_present_total_idle = true;
    if (this_present_total_idle || that_present_total_idle) {
      if (!(this_present_total_idle && that_present_total_idle))
        return false;
      if (this.total_idle != that.total_idle)
        return false;
    }

    boolean this_present_total_busy = true;
    boolean that_present_total_busy = true;
    if (this_present_total_busy || that_present_total_busy) {
      if (!(this_present_total_busy && that_present_total_busy))
        return false;
      if (this.total_busy != that.total_busy)
        return false;
    }

    boolean this_present_ncompleted = true;
    boolean that_present_ncompleted = true;
    if (this_present_ncompleted || that_present_ncompleted) {
      if (!(this_present_ncompleted && that_present_ncompleted))
        return false;
      if (this.ncompleted != that.ncompleted)
        return false;
    }

    boolean this_present_wkcompleted = true;
    boolean that_present_wkcompleted = true;
    if (this_present_wkcompleted || that_present_wkcompleted) {
      if (!(this_present_wkcompleted && that_present_wkcompleted))
        return false;
      if (this.wkcompleted != that.wkcompleted)
        return false;
    }

    boolean this_present_ctime = true;
    boolean that_present_ctime = true;
    if (this_present_ctime || that_present_ctime) {
      if (!(this_present_ctime && that_present_ctime))
        return false;
      if (this.ctime != that.ctime)
        return false;
    }

    boolean this_present_status = true;
    boolean that_present_status = true;
    if (this_present_status || that_present_status) {
      if (!(this_present_status && that_present_status))
        return false;
      if (this.status != that.status)
        return false;
    }

    boolean this_present_mtime = true;
    boolean that_present_mtime = true;
    if (this_present_mtime || that_present_mtime) {
      if (!(this_present_mtime && that_present_mtime))
        return false;
      if (this.mtime != that.mtime)
        return false;
    }

    boolean this_present_svridmax = true;
    boolean that_present_svridmax = true;
    if (this_present_svridmax || that_present_svridmax) {
      if (!(this_present_svridmax && that_present_svridmax))
        return false;
      if (this.svridmax != that.svridmax)
        return false;
    }

    return true;
  }

  @Override
  public int hashCode() {
    return 0;
  }

  public int compareTo(ServerState other) {
    if (!getClass().equals(other.getClass())) {
      return getClass().getName().compareTo(other.getClass().getName());
    }

    int lastComparison = 0;
    ServerState typedOther = (ServerState)other;

    lastComparison = Boolean.valueOf(isSetSgid()).compareTo(typedOther.isSetSgid());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetSgid()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.sgid, typedOther.sgid);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetServerid()).compareTo(typedOther.isSetServerid());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetServerid()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.serverid, typedOther.serverid);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetTotal_idle()).compareTo(typedOther.isSetTotal_idle());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetTotal_idle()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.total_idle, typedOther.total_idle);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetTotal_busy()).compareTo(typedOther.isSetTotal_busy());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetTotal_busy()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.total_busy, typedOther.total_busy);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetNcompleted()).compareTo(typedOther.isSetNcompleted());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetNcompleted()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.ncompleted, typedOther.ncompleted);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetWkcompleted()).compareTo(typedOther.isSetWkcompleted());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetWkcompleted()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.wkcompleted, typedOther.wkcompleted);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetCtime()).compareTo(typedOther.isSetCtime());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetCtime()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.ctime, typedOther.ctime);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetStatus()).compareTo(typedOther.isSetStatus());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetStatus()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.status, typedOther.status);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetMtime()).compareTo(typedOther.isSetMtime());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetMtime()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.mtime, typedOther.mtime);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetSvridmax()).compareTo(typedOther.isSetSvridmax());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetSvridmax()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.svridmax, typedOther.svridmax);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    return 0;
  }

  public _Fields fieldForId(int fieldId) {
    return _Fields.findByThriftId(fieldId);
  }

  public void read(org.apache.thrift.protocol.TProtocol iprot) throws org.apache.thrift.TException {
    schemes.get(iprot.getScheme()).getScheme().read(iprot, this);
  }

  public void write(org.apache.thrift.protocol.TProtocol oprot) throws org.apache.thrift.TException {
    schemes.get(oprot.getScheme()).getScheme().write(oprot, this);
  }

  @Override
  public String toString() {
    StringBuilder sb = new StringBuilder("ServerState(");
    boolean first = true;

    sb.append("sgid:");
    sb.append(this.sgid);
    first = false;
    if (!first) sb.append(", ");
    sb.append("serverid:");
    sb.append(this.serverid);
    first = false;
    if (!first) sb.append(", ");
    sb.append("total_idle:");
    sb.append(this.total_idle);
    first = false;
    if (!first) sb.append(", ");
    sb.append("total_busy:");
    sb.append(this.total_busy);
    first = false;
    if (!first) sb.append(", ");
    sb.append("ncompleted:");
    sb.append(this.ncompleted);
    first = false;
    if (!first) sb.append(", ");
    sb.append("wkcompleted:");
    sb.append(this.wkcompleted);
    first = false;
    if (!first) sb.append(", ");
    sb.append("ctime:");
    sb.append(this.ctime);
    first = false;
    if (!first) sb.append(", ");
    sb.append("status:");
    sb.append(this.status);
    first = false;
    if (!first) sb.append(", ");
    sb.append("mtime:");
    sb.append(this.mtime);
    first = false;
    if (!first) sb.append(", ");
    sb.append("svridmax:");
    sb.append(this.svridmax);
    first = false;
    sb.append(")");
    return sb.toString();
  }

  public void validate() throws org.apache.thrift.TException {
    // check for required fields
  }

  private void writeObject(java.io.ObjectOutputStream out) throws java.io.IOException {
    try {
      write(new org.apache.thrift.protocol.TCompactProtocol(new org.apache.thrift.transport.TIOStreamTransport(out)));
    } catch (org.apache.thrift.TException te) {
      throw new java.io.IOException(te);
    }
  }

  private void readObject(java.io.ObjectInputStream in) throws java.io.IOException, ClassNotFoundException {
    try {
      // it doesn't seem like you should have to do this, but java serialization is wacky, and doesn't call the default constructor.
      __isset_bit_vector = new BitSet(1);
      read(new org.apache.thrift.protocol.TCompactProtocol(new org.apache.thrift.transport.TIOStreamTransport(in)));
    } catch (org.apache.thrift.TException te) {
      throw new java.io.IOException(te);
    }
  }

  private static class ServerStateStandardSchemeFactory implements SchemeFactory {
    public ServerStateStandardScheme getScheme() {
      return new ServerStateStandardScheme();
    }
  }

  private static class ServerStateStandardScheme extends StandardScheme<ServerState> {

    public void read(org.apache.thrift.protocol.TProtocol iprot, ServerState struct) throws org.apache.thrift.TException {
      org.apache.thrift.protocol.TField schemeField;
      iprot.readStructBegin();
      while (true)
      {
        schemeField = iprot.readFieldBegin();
        if (schemeField.type == org.apache.thrift.protocol.TType.STOP) { 
          break;
        }
        switch (schemeField.id) {
          case 1: // SGID
            if (schemeField.type == org.apache.thrift.protocol.TType.I64) {
              struct.sgid = iprot.readI64();
              struct.setSgidIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 2: // SERVERID
            if (schemeField.type == org.apache.thrift.protocol.TType.I32) {
              struct.serverid = iprot.readI32();
              struct.setServeridIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 3: // TOTAL_IDLE
            if (schemeField.type == org.apache.thrift.protocol.TType.I32) {
              struct.total_idle = iprot.readI32();
              struct.setTotal_idleIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 4: // TOTAL_BUSY
            if (schemeField.type == org.apache.thrift.protocol.TType.I32) {
              struct.total_busy = iprot.readI32();
              struct.setTotal_busyIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 5: // NCOMPLETED
            if (schemeField.type == org.apache.thrift.protocol.TType.I64) {
              struct.ncompleted = iprot.readI64();
              struct.setNcompletedIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 6: // WKCOMPLETED
            if (schemeField.type == org.apache.thrift.protocol.TType.I64) {
              struct.wkcompleted = iprot.readI64();
              struct.setWkcompletedIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 7: // CTIME
            if (schemeField.type == org.apache.thrift.protocol.TType.I64) {
              struct.ctime = iprot.readI64();
              struct.setCtimeIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 8: // STATUS
            if (schemeField.type == org.apache.thrift.protocol.TType.I32) {
              struct.status = iprot.readI32();
              struct.setStatusIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 9: // MTIME
            if (schemeField.type == org.apache.thrift.protocol.TType.I64) {
              struct.mtime = iprot.readI64();
              struct.setMtimeIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 10: // SVRIDMAX
            if (schemeField.type == org.apache.thrift.protocol.TType.I64) {
              struct.svridmax = iprot.readI64();
              struct.setSvridmaxIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          default:
            org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
        }
        iprot.readFieldEnd();
      }
      iprot.readStructEnd();

      // check for required fields of primitive type, which can't be checked in the validate method
      struct.validate();
    }

    public void write(org.apache.thrift.protocol.TProtocol oprot, ServerState struct) throws org.apache.thrift.TException {
      struct.validate();

      oprot.writeStructBegin(STRUCT_DESC);
      oprot.writeFieldBegin(SGID_FIELD_DESC);
      oprot.writeI64(struct.sgid);
      oprot.writeFieldEnd();
      oprot.writeFieldBegin(SERVERID_FIELD_DESC);
      oprot.writeI32(struct.serverid);
      oprot.writeFieldEnd();
      oprot.writeFieldBegin(TOTAL_IDLE_FIELD_DESC);
      oprot.writeI32(struct.total_idle);
      oprot.writeFieldEnd();
      oprot.writeFieldBegin(TOTAL_BUSY_FIELD_DESC);
      oprot.writeI32(struct.total_busy);
      oprot.writeFieldEnd();
      oprot.writeFieldBegin(NCOMPLETED_FIELD_DESC);
      oprot.writeI64(struct.ncompleted);
      oprot.writeFieldEnd();
      oprot.writeFieldBegin(WKCOMPLETED_FIELD_DESC);
      oprot.writeI64(struct.wkcompleted);
      oprot.writeFieldEnd();
      oprot.writeFieldBegin(CTIME_FIELD_DESC);
      oprot.writeI64(struct.ctime);
      oprot.writeFieldEnd();
      oprot.writeFieldBegin(STATUS_FIELD_DESC);
      oprot.writeI32(struct.status);
      oprot.writeFieldEnd();
      oprot.writeFieldBegin(MTIME_FIELD_DESC);
      oprot.writeI64(struct.mtime);
      oprot.writeFieldEnd();
      oprot.writeFieldBegin(SVRIDMAX_FIELD_DESC);
      oprot.writeI64(struct.svridmax);
      oprot.writeFieldEnd();
      oprot.writeFieldStop();
      oprot.writeStructEnd();
    }

  }

  private static class ServerStateTupleSchemeFactory implements SchemeFactory {
    public ServerStateTupleScheme getScheme() {
      return new ServerStateTupleScheme();
    }
  }

  private static class ServerStateTupleScheme extends TupleScheme<ServerState> {

    @Override
    public void write(org.apache.thrift.protocol.TProtocol prot, ServerState struct) throws org.apache.thrift.TException {
      TTupleProtocol oprot = (TTupleProtocol) prot;
      BitSet optionals = new BitSet();
      if (struct.isSetSgid()) {
        optionals.set(0);
      }
      if (struct.isSetServerid()) {
        optionals.set(1);
      }
      if (struct.isSetTotal_idle()) {
        optionals.set(2);
      }
      if (struct.isSetTotal_busy()) {
        optionals.set(3);
      }
      if (struct.isSetNcompleted()) {
        optionals.set(4);
      }
      if (struct.isSetWkcompleted()) {
        optionals.set(5);
      }
      if (struct.isSetCtime()) {
        optionals.set(6);
      }
      if (struct.isSetStatus()) {
        optionals.set(7);
      }
      if (struct.isSetMtime()) {
        optionals.set(8);
      }
      if (struct.isSetSvridmax()) {
        optionals.set(9);
      }
      oprot.writeBitSet(optionals, 10);
      if (struct.isSetSgid()) {
        oprot.writeI64(struct.sgid);
      }
      if (struct.isSetServerid()) {
        oprot.writeI32(struct.serverid);
      }
      if (struct.isSetTotal_idle()) {
        oprot.writeI32(struct.total_idle);
      }
      if (struct.isSetTotal_busy()) {
        oprot.writeI32(struct.total_busy);
      }
      if (struct.isSetNcompleted()) {
        oprot.writeI64(struct.ncompleted);
      }
      if (struct.isSetWkcompleted()) {
        oprot.writeI64(struct.wkcompleted);
      }
      if (struct.isSetCtime()) {
        oprot.writeI64(struct.ctime);
      }
      if (struct.isSetStatus()) {
        oprot.writeI32(struct.status);
      }
      if (struct.isSetMtime()) {
        oprot.writeI64(struct.mtime);
      }
      if (struct.isSetSvridmax()) {
        oprot.writeI64(struct.svridmax);
      }
    }

    @Override
    public void read(org.apache.thrift.protocol.TProtocol prot, ServerState struct) throws org.apache.thrift.TException {
      TTupleProtocol iprot = (TTupleProtocol) prot;
      BitSet incoming = iprot.readBitSet(10);
      if (incoming.get(0)) {
        struct.sgid = iprot.readI64();
        struct.setSgidIsSet(true);
      }
      if (incoming.get(1)) {
        struct.serverid = iprot.readI32();
        struct.setServeridIsSet(true);
      }
      if (incoming.get(2)) {
        struct.total_idle = iprot.readI32();
        struct.setTotal_idleIsSet(true);
      }
      if (incoming.get(3)) {
        struct.total_busy = iprot.readI32();
        struct.setTotal_busyIsSet(true);
      }
      if (incoming.get(4)) {
        struct.ncompleted = iprot.readI64();
        struct.setNcompletedIsSet(true);
      }
      if (incoming.get(5)) {
        struct.wkcompleted = iprot.readI64();
        struct.setWkcompletedIsSet(true);
      }
      if (incoming.get(6)) {
        struct.ctime = iprot.readI64();
        struct.setCtimeIsSet(true);
      }
      if (incoming.get(7)) {
        struct.status = iprot.readI32();
        struct.setStatusIsSet(true);
      }
      if (incoming.get(8)) {
        struct.mtime = iprot.readI64();
        struct.setMtimeIsSet(true);
      }
      if (incoming.get(9)) {
        struct.svridmax = iprot.readI64();
        struct.setSvridmaxIsSet(true);
      }
    }
  }

}

