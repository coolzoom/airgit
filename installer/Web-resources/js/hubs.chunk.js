webpackJsonp([17],{574:function(t,e,n){"use strict";function r(t){return t&&t.__esModule?t:{"default":t}}var a=Object.assign||function(t){for(var e=1;e<arguments.length;e++){var n=arguments[e];for(var r in n)Object.prototype.hasOwnProperty.call(n,r)&&(t[r]=n[r])}return t};Object.defineProperty(e,"__esModule",{value:!0});var i=n(1),u=r(i),o=n(3),s=r(o),c=n(133),l=r(c),d=n(126),f=r(d),p=n(73),m=r(p),h=n(56),b=r(h),v=n(49),_=r(v),g=n(5),I=r(g),y=n(30),S=n(74),E={itemNameGetter:function(t){return t.identity.name},itemStatusGetter:function(t){return _["default"].hubOnlineStatusToColor(t.connect_state.id)},itemDescriptionGetter:function(t){return u["default"].createElement(l["default"],{properties:{target:"_blank"}},t.identity.description)},itemIconGetter:function(t){return u["default"].createElement(S.HubIconFormatter,{size:"large",hub:t})},itemHeaderGetter:function(t,e,n){return u["default"].createElement(y.ActionMenu,{location:e,caption:t.identity.name,actions:b["default"],itemData:t,ids:["reconnect","favorite"]},n)}},G=u["default"].createClass({displayName:"Hubs",mixins:[s["default"].connect(m["default"],"hubSessions")],_getActiveId:function(){return this.props.params?parseInt(this.props.params.id):null},render:function(){return u["default"].createElement(f["default"],a({activeId:this._getActiveId(),baseUrl:"hubs",itemUrl:"hubs/session",location:this.props.location,items:this.state.hubSessions,newButtonCaption:"Connect",editAccess:I["default"].HUBS_EDIT,actions:b["default"],unreadInfoStore:m["default"]},E),this.props.children)}});e["default"]=G}});
//# sourceMappingURL=hubs.chunk.js.map