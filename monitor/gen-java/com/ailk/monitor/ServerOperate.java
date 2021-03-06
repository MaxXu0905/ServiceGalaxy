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

public class ServerOperate implements org.apache.thrift.TBase<ServerOperate, ServerOperate._Fields>, java.io.Serializable, Cloneable {
  private static final org.apache.thrift.protocol.TStruct STRUCT_DESC = new org.apache.thrift.protocol.TStruct("ServerOperate");

  private static final org.apache.thrift.protocol.TField TYPE_FIELD_DESC = new org.apache.thrift.protocol.TField("type", org.apache.thrift.protocol.TType.I32, (short)1);
  private static final org.apache.thrift.protocol.TField OPERATE_FIELD_DESC = new org.apache.thrift.protocol.TField("operate", org.apache.thrift.protocol.TType.I32, (short)2);
  private static final org.apache.thrift.protocol.TField GROUP_ID_FIELD_DESC = new org.apache.thrift.protocol.TField("groupId", org.apache.thrift.protocol.TType.I32, (short)3);
  private static final org.apache.thrift.protocol.TField SERVER_ID_FIELD_DESC = new org.apache.thrift.protocol.TField("serverId", org.apache.thrift.protocol.TType.I32, (short)4);
  private static final org.apache.thrift.protocol.TField MACHINE_ID_FIELD_DESC = new org.apache.thrift.protocol.TField("machineId", org.apache.thrift.protocol.TType.STRING, (short)5);

  private static final Map<Class<? extends IScheme>, SchemeFactory> schemes = new HashMap<Class<? extends IScheme>, SchemeFactory>();
  static {
    schemes.put(StandardScheme.class, new ServerOperateStandardSchemeFactory());
    schemes.put(TupleScheme.class, new ServerOperateTupleSchemeFactory());
  }

  /**
   * 
   * @see OperateType
   */
  public OperateType type; // required
  /**
   * 
   * @see Operate
   */
  public Operate operate; // required
  public int groupId; // optional
  public int serverId; // optional
  public String machineId; // optional

  /** The set of fields this struct contains, along with convenience methods for finding and manipulating them. */
  public enum _Fields implements org.apache.thrift.TFieldIdEnum {
    /**
     * 
     * @see OperateType
     */
    TYPE((short)1, "type"),
    /**
     * 
     * @see Operate
     */
    OPERATE((short)2, "operate"),
    GROUP_ID((short)3, "groupId"),
    SERVER_ID((short)4, "serverId"),
    MACHINE_ID((short)5, "machineId");

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
        case 1: // TYPE
          return TYPE;
        case 2: // OPERATE
          return OPERATE;
        case 3: // GROUP_ID
          return GROUP_ID;
        case 4: // SERVER_ID
          return SERVER_ID;
        case 5: // MACHINE_ID
          return MACHINE_ID;
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
  private static final int __GROUPID_ISSET_ID = 0;
  private static final int __SERVERID_ISSET_ID = 1;
  private BitSet __isset_bit_vector = new BitSet(2);
  private _Fields optionals[] = {_Fields.GROUP_ID,_Fields.SERVER_ID,_Fields.MACHINE_ID};
  public static final Map<_Fields, org.apache.thrift.meta_data.FieldMetaData> metaDataMap;
  static {
    Map<_Fields, org.apache.thrift.meta_data.FieldMetaData> tmpMap = new EnumMap<_Fields, org.apache.thrift.meta_data.FieldMetaData>(_Fields.class);
    tmpMap.put(_Fields.TYPE, new org.apache.thrift.meta_data.FieldMetaData("type", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.EnumMetaData(org.apache.thrift.protocol.TType.ENUM, OperateType.class)));
    tmpMap.put(_Fields.OPERATE, new org.apache.thrift.meta_data.FieldMetaData("operate", org.apache.thrift.TFieldRequirementType.DEFAULT, 
        new org.apache.thrift.meta_data.EnumMetaData(org.apache.thrift.protocol.TType.ENUM, Operate.class)));
    tmpMap.put(_Fields.GROUP_ID, new org.apache.thrift.meta_data.FieldMetaData("groupId", org.apache.thrift.TFieldRequirementType.OPTIONAL, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I32)));
    tmpMap.put(_Fields.SERVER_ID, new org.apache.thrift.meta_data.FieldMetaData("serverId", org.apache.thrift.TFieldRequirementType.OPTIONAL, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I32)));
    tmpMap.put(_Fields.MACHINE_ID, new org.apache.thrift.meta_data.FieldMetaData("machineId", org.apache.thrift.TFieldRequirementType.OPTIONAL, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.STRING)));
    metaDataMap = Collections.unmodifiableMap(tmpMap);
    org.apache.thrift.meta_data.FieldMetaData.addStructMetaDataMap(ServerOperate.class, metaDataMap);
  }

  public ServerOperate() {
  }

  public ServerOperate(
    OperateType type,
    Operate operate)
  {
    this();
    this.type = type;
    this.operate = operate;
  }

  /**
   * Performs a deep copy on <i>other</i>.
   */
  public ServerOperate(ServerOperate other) {
    __isset_bit_vector.clear();
    __isset_bit_vector.or(other.__isset_bit_vector);
    if (other.isSetType()) {
      this.type = other.type;
    }
    if (other.isSetOperate()) {
      this.operate = other.operate;
    }
    this.groupId = other.groupId;
    this.serverId = other.serverId;
    if (other.isSetMachineId()) {
      this.machineId = other.machineId;
    }
  }

  public ServerOperate deepCopy() {
    return new ServerOperate(this);
  }

  @Override
  public void clear() {
    this.type = null;
    this.operate = null;
    setGroupIdIsSet(false);
    this.groupId = 0;
    setServerIdIsSet(false);
    this.serverId = 0;
    this.machineId = null;
  }

  /**
   * 
   * @see OperateType
   */
  public OperateType getType() {
    return this.type;
  }

  /**
   * 
   * @see OperateType
   */
  public ServerOperate setType(OperateType type) {
    this.type = type;
    return this;
  }

  public void unsetType() {
    this.type = null;
  }

  /** Returns true if field type is set (has been assigned a value) and false otherwise */
  public boolean isSetType() {
    return this.type != null;
  }

  public void setTypeIsSet(boolean value) {
    if (!value) {
      this.type = null;
    }
  }

  /**
   * 
   * @see Operate
   */
  public Operate getOperate() {
    return this.operate;
  }

  /**
   * 
   * @see Operate
   */
  public ServerOperate setOperate(Operate operate) {
    this.operate = operate;
    return this;
  }

  public void unsetOperate() {
    this.operate = null;
  }

  /** Returns true if field operate is set (has been assigned a value) and false otherwise */
  public boolean isSetOperate() {
    return this.operate != null;
  }

  public void setOperateIsSet(boolean value) {
    if (!value) {
      this.operate = null;
    }
  }

  public int getGroupId() {
    return this.groupId;
  }

  public ServerOperate setGroupId(int groupId) {
    this.groupId = groupId;
    setGroupIdIsSet(true);
    return this;
  }

  public void unsetGroupId() {
    __isset_bit_vector.clear(__GROUPID_ISSET_ID);
  }

  /** Returns true if field groupId is set (has been assigned a value) and false otherwise */
  public boolean isSetGroupId() {
    return __isset_bit_vector.get(__GROUPID_ISSET_ID);
  }

  public void setGroupIdIsSet(boolean value) {
    __isset_bit_vector.set(__GROUPID_ISSET_ID, value);
  }

  public int getServerId() {
    return this.serverId;
  }

  public ServerOperate setServerId(int serverId) {
    this.serverId = serverId;
    setServerIdIsSet(true);
    return this;
  }

  public void unsetServerId() {
    __isset_bit_vector.clear(__SERVERID_ISSET_ID);
  }

  /** Returns true if field serverId is set (has been assigned a value) and false otherwise */
  public boolean isSetServerId() {
    return __isset_bit_vector.get(__SERVERID_ISSET_ID);
  }

  public void setServerIdIsSet(boolean value) {
    __isset_bit_vector.set(__SERVERID_ISSET_ID, value);
  }

  public String getMachineId() {
    return this.machineId;
  }

  public ServerOperate setMachineId(String machineId) {
    this.machineId = machineId;
    return this;
  }

  public void unsetMachineId() {
    this.machineId = null;
  }

  /** Returns true if field machineId is set (has been assigned a value) and false otherwise */
  public boolean isSetMachineId() {
    return this.machineId != null;
  }

  public void setMachineIdIsSet(boolean value) {
    if (!value) {
      this.machineId = null;
    }
  }

  public void setFieldValue(_Fields field, Object value) {
    switch (field) {
    case TYPE:
      if (value == null) {
        unsetType();
      } else {
        setType((OperateType)value);
      }
      break;

    case OPERATE:
      if (value == null) {
        unsetOperate();
      } else {
        setOperate((Operate)value);
      }
      break;

    case GROUP_ID:
      if (value == null) {
        unsetGroupId();
      } else {
        setGroupId((Integer)value);
      }
      break;

    case SERVER_ID:
      if (value == null) {
        unsetServerId();
      } else {
        setServerId((Integer)value);
      }
      break;

    case MACHINE_ID:
      if (value == null) {
        unsetMachineId();
      } else {
        setMachineId((String)value);
      }
      break;

    }
  }

  public Object getFieldValue(_Fields field) {
    switch (field) {
    case TYPE:
      return getType();

    case OPERATE:
      return getOperate();

    case GROUP_ID:
      return Integer.valueOf(getGroupId());

    case SERVER_ID:
      return Integer.valueOf(getServerId());

    case MACHINE_ID:
      return getMachineId();

    }
    throw new IllegalStateException();
  }

  /** Returns true if field corresponding to fieldID is set (has been assigned a value) and false otherwise */
  public boolean isSet(_Fields field) {
    if (field == null) {
      throw new IllegalArgumentException();
    }

    switch (field) {
    case TYPE:
      return isSetType();
    case OPERATE:
      return isSetOperate();
    case GROUP_ID:
      return isSetGroupId();
    case SERVER_ID:
      return isSetServerId();
    case MACHINE_ID:
      return isSetMachineId();
    }
    throw new IllegalStateException();
  }

  @Override
  public boolean equals(Object that) {
    if (that == null)
      return false;
    if (that instanceof ServerOperate)
      return this.equals((ServerOperate)that);
    return false;
  }

  public boolean equals(ServerOperate that) {
    if (that == null)
      return false;

    boolean this_present_type = true && this.isSetType();
    boolean that_present_type = true && that.isSetType();
    if (this_present_type || that_present_type) {
      if (!(this_present_type && that_present_type))
        return false;
      if (!this.type.equals(that.type))
        return false;
    }

    boolean this_present_operate = true && this.isSetOperate();
    boolean that_present_operate = true && that.isSetOperate();
    if (this_present_operate || that_present_operate) {
      if (!(this_present_operate && that_present_operate))
        return false;
      if (!this.operate.equals(that.operate))
        return false;
    }

    boolean this_present_groupId = true && this.isSetGroupId();
    boolean that_present_groupId = true && that.isSetGroupId();
    if (this_present_groupId || that_present_groupId) {
      if (!(this_present_groupId && that_present_groupId))
        return false;
      if (this.groupId != that.groupId)
        return false;
    }

    boolean this_present_serverId = true && this.isSetServerId();
    boolean that_present_serverId = true && that.isSetServerId();
    if (this_present_serverId || that_present_serverId) {
      if (!(this_present_serverId && that_present_serverId))
        return false;
      if (this.serverId != that.serverId)
        return false;
    }

    boolean this_present_machineId = true && this.isSetMachineId();
    boolean that_present_machineId = true && that.isSetMachineId();
    if (this_present_machineId || that_present_machineId) {
      if (!(this_present_machineId && that_present_machineId))
        return false;
      if (!this.machineId.equals(that.machineId))
        return false;
    }

    return true;
  }

  @Override
  public int hashCode() {
    return 0;
  }

  public int compareTo(ServerOperate other) {
    if (!getClass().equals(other.getClass())) {
      return getClass().getName().compareTo(other.getClass().getName());
    }

    int lastComparison = 0;
    ServerOperate typedOther = (ServerOperate)other;

    lastComparison = Boolean.valueOf(isSetType()).compareTo(typedOther.isSetType());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetType()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.type, typedOther.type);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetOperate()).compareTo(typedOther.isSetOperate());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetOperate()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.operate, typedOther.operate);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetGroupId()).compareTo(typedOther.isSetGroupId());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetGroupId()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.groupId, typedOther.groupId);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetServerId()).compareTo(typedOther.isSetServerId());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetServerId()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.serverId, typedOther.serverId);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetMachineId()).compareTo(typedOther.isSetMachineId());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetMachineId()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.machineId, typedOther.machineId);
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
    StringBuilder sb = new StringBuilder("ServerOperate(");
    boolean first = true;

    sb.append("type:");
    if (this.type == null) {
      sb.append("null");
    } else {
      sb.append(this.type);
    }
    first = false;
    if (!first) sb.append(", ");
    sb.append("operate:");
    if (this.operate == null) {
      sb.append("null");
    } else {
      sb.append(this.operate);
    }
    first = false;
    if (isSetGroupId()) {
      if (!first) sb.append(", ");
      sb.append("groupId:");
      sb.append(this.groupId);
      first = false;
    }
    if (isSetServerId()) {
      if (!first) sb.append(", ");
      sb.append("serverId:");
      sb.append(this.serverId);
      first = false;
    }
    if (isSetMachineId()) {
      if (!first) sb.append(", ");
      sb.append("machineId:");
      if (this.machineId == null) {
        sb.append("null");
      } else {
        sb.append(this.machineId);
      }
      first = false;
    }
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

  private static class ServerOperateStandardSchemeFactory implements SchemeFactory {
    public ServerOperateStandardScheme getScheme() {
      return new ServerOperateStandardScheme();
    }
  }

  private static class ServerOperateStandardScheme extends StandardScheme<ServerOperate> {

    public void read(org.apache.thrift.protocol.TProtocol iprot, ServerOperate struct) throws org.apache.thrift.TException {
      org.apache.thrift.protocol.TField schemeField;
      iprot.readStructBegin();
      while (true)
      {
        schemeField = iprot.readFieldBegin();
        if (schemeField.type == org.apache.thrift.protocol.TType.STOP) { 
          break;
        }
        switch (schemeField.id) {
          case 1: // TYPE
            if (schemeField.type == org.apache.thrift.protocol.TType.I32) {
              struct.type = OperateType.findByValue(iprot.readI32());
              struct.setTypeIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 2: // OPERATE
            if (schemeField.type == org.apache.thrift.protocol.TType.I32) {
              struct.operate = Operate.findByValue(iprot.readI32());
              struct.setOperateIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 3: // GROUP_ID
            if (schemeField.type == org.apache.thrift.protocol.TType.I32) {
              struct.groupId = iprot.readI32();
              struct.setGroupIdIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 4: // SERVER_ID
            if (schemeField.type == org.apache.thrift.protocol.TType.I32) {
              struct.serverId = iprot.readI32();
              struct.setServerIdIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 5: // MACHINE_ID
            if (schemeField.type == org.apache.thrift.protocol.TType.STRING) {
              struct.machineId = iprot.readString();
              struct.setMachineIdIsSet(true);
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

    public void write(org.apache.thrift.protocol.TProtocol oprot, ServerOperate struct) throws org.apache.thrift.TException {
      struct.validate();

      oprot.writeStructBegin(STRUCT_DESC);
      if (struct.type != null) {
        oprot.writeFieldBegin(TYPE_FIELD_DESC);
        oprot.writeI32(struct.type.getValue());
        oprot.writeFieldEnd();
      }
      if (struct.operate != null) {
        oprot.writeFieldBegin(OPERATE_FIELD_DESC);
        oprot.writeI32(struct.operate.getValue());
        oprot.writeFieldEnd();
      }
      if (struct.isSetGroupId()) {
        oprot.writeFieldBegin(GROUP_ID_FIELD_DESC);
        oprot.writeI32(struct.groupId);
        oprot.writeFieldEnd();
      }
      if (struct.isSetServerId()) {
        oprot.writeFieldBegin(SERVER_ID_FIELD_DESC);
        oprot.writeI32(struct.serverId);
        oprot.writeFieldEnd();
      }
      if (struct.machineId != null) {
        if (struct.isSetMachineId()) {
          oprot.writeFieldBegin(MACHINE_ID_FIELD_DESC);
          oprot.writeString(struct.machineId);
          oprot.writeFieldEnd();
        }
      }
      oprot.writeFieldStop();
      oprot.writeStructEnd();
    }

  }

  private static class ServerOperateTupleSchemeFactory implements SchemeFactory {
    public ServerOperateTupleScheme getScheme() {
      return new ServerOperateTupleScheme();
    }
  }

  private static class ServerOperateTupleScheme extends TupleScheme<ServerOperate> {

    @Override
    public void write(org.apache.thrift.protocol.TProtocol prot, ServerOperate struct) throws org.apache.thrift.TException {
      TTupleProtocol oprot = (TTupleProtocol) prot;
      BitSet optionals = new BitSet();
      if (struct.isSetType()) {
        optionals.set(0);
      }
      if (struct.isSetOperate()) {
        optionals.set(1);
      }
      if (struct.isSetGroupId()) {
        optionals.set(2);
      }
      if (struct.isSetServerId()) {
        optionals.set(3);
      }
      if (struct.isSetMachineId()) {
        optionals.set(4);
      }
      oprot.writeBitSet(optionals, 5);
      if (struct.isSetType()) {
        oprot.writeI32(struct.type.getValue());
      }
      if (struct.isSetOperate()) {
        oprot.writeI32(struct.operate.getValue());
      }
      if (struct.isSetGroupId()) {
        oprot.writeI32(struct.groupId);
      }
      if (struct.isSetServerId()) {
        oprot.writeI32(struct.serverId);
      }
      if (struct.isSetMachineId()) {
        oprot.writeString(struct.machineId);
      }
    }

    @Override
    public void read(org.apache.thrift.protocol.TProtocol prot, ServerOperate struct) throws org.apache.thrift.TException {
      TTupleProtocol iprot = (TTupleProtocol) prot;
      BitSet incoming = iprot.readBitSet(5);
      if (incoming.get(0)) {
        struct.type = OperateType.findByValue(iprot.readI32());
        struct.setTypeIsSet(true);
      }
      if (incoming.get(1)) {
        struct.operate = Operate.findByValue(iprot.readI32());
        struct.setOperateIsSet(true);
      }
      if (incoming.get(2)) {
        struct.groupId = iprot.readI32();
        struct.setGroupIdIsSet(true);
      }
      if (incoming.get(3)) {
        struct.serverId = iprot.readI32();
        struct.setServerIdIsSet(true);
      }
      if (incoming.get(4)) {
        struct.machineId = iprot.readString();
        struct.setMachineIdIsSet(true);
      }
    }
  }

}

